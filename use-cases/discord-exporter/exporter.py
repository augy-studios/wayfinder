"""
Discord channel exporter — crawls every accessible guild/channel and
writes messages + attachments to an HTML or plain-text archive.

Usage:
    python exporter.py --token YOUR_BOT_TOKEN --format html --out ./export
    python exporter.py --token YOUR_BOT_TOKEN --format txt  --out ./export
    # To export a single guild:
    python exporter.py --token TOKEN --guild GUILD_ID --format html --out ./export

Requirements:
    pip install discord.py aiohttp aiofiles

Token note:
    Use a Bot token (from discord.com/developers). The bot must have:
      - Read Messages / View Channels
      - Read Message History
    in each target server. Invite the bot with:
      https://discord.com/api/oauth2/authorize?client_id=BOT_ID&permissions=68608&scope=bot
"""

import asyncio
import argparse
import os
import re
import html as html_lib
from datetime import datetime, timezone
from pathlib import Path

import discord
import aiohttp
import aiofiles


# ---------------------------------------------------------------------------
# Sanitisation helpers
# ---------------------------------------------------------------------------

def safe_name(name: str) -> str:
    """Convert a guild/channel name to a safe filesystem name."""
    return re.sub(r'[^\w\-]', '_', name)[:64]


def fmt_timestamp(dt: datetime) -> str:
    if dt is None:
        return ''
    return dt.astimezone(timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')


# ---------------------------------------------------------------------------
# HTML template
# ---------------------------------------------------------------------------

HTML_HEAD = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
  body {{ font-family: sans-serif; background:#36393f; color:#dcddde; margin:0; padding:1em 2em; }}
  h1,h2 {{ color:#fff; }}
  .msg {{ border-left:3px solid #5865f2; margin:.4em 0; padding:.3em .6em; background:#2f3136; border-radius:4px; }}
  .meta {{ font-size:.8em; color:#72767d; }}
  .author {{ font-weight:bold; color:#7289da; }}
  .attach {{ margin:.2em 0; font-size:.85em; }}
  a {{ color:#00b0f4; }}
</style>
</head>
<body>
<h1>{title}</h1>
<p class="meta">Exported {ts}</p>
"""

HTML_FOOT = "</body></html>\n"


# ---------------------------------------------------------------------------
# Attachment downloader
# ---------------------------------------------------------------------------

async def download_attachment(session: aiohttp.ClientSession,
                               url: str, dest: Path) -> None:
    try:
        async with session.get(url) as r:
            if r.status == 200:
                dest.parent.mkdir(parents=True, exist_ok=True)
                async with aiofiles.open(dest, 'wb') as f:
                    await f.write(await r.read())
    except Exception:
        pass  # attachment may have expired; skip silently


# ---------------------------------------------------------------------------
# Per-channel export
# ---------------------------------------------------------------------------

async def export_channel_html(channel: discord.TextChannel,
                               out_dir: Path,
                               session: aiohttp.ClientSession) -> int:
    """Write <channel_name>.html and download attachments. Returns message count."""
    fname = out_dir / f"{safe_name(channel.name)}.html"
    attach_dir = out_dir / "attachments" / safe_name(channel.name)

    title = f"#{channel.name} — {channel.guild.name}"
    count = 0

    async with aiofiles.open(fname, 'w', encoding='utf-8') as f:
        await f.write(HTML_HEAD.format(
            title=html_lib.escape(title),
            ts=fmt_timestamp(datetime.now(timezone.utc))
        ))
        await f.write(f'<h2>{html_lib.escape(title)}</h2>\n')

        try:
            async for msg in channel.history(limit=None, oldest_first=True):
                count += 1
                author = html_lib.escape(str(msg.author))
                ts     = fmt_timestamp(msg.created_at)
                body   = html_lib.escape(msg.content or '')

                await f.write(
                    f'<div class="msg">'
                    f'<span class="author">{author}</span> '
                    f'<span class="meta">{ts}</span><br>'
                    f'{body}\n'
                )

                for att in msg.attachments:
                    ext  = Path(att.filename).suffix
                    dest = attach_dir / f"{msg.id}_{att.id}{ext}"
                    rel  = dest.relative_to(out_dir)
                    await f.write(
                        f'<div class="attach">📎 '
                        f'<a href="{rel}">{html_lib.escape(att.filename)}</a>'
                        f' ({att.size:,} bytes)</div>\n'
                    )
                    await download_attachment(session, att.url, dest)

                await f.write('</div>\n')
        except discord.Forbidden:
            await f.write('<p><em>No permission to read this channel.</em></p>\n')

        await f.write(HTML_FOOT)

    return count


async def export_channel_txt(channel: discord.TextChannel,
                              out_dir: Path,
                              session: aiohttp.ClientSession) -> int:
    fname = out_dir / f"{safe_name(channel.name)}.txt"
    attach_dir = out_dir / "attachments" / safe_name(channel.name)
    count = 0

    async with aiofiles.open(fname, 'w', encoding='utf-8') as f:
        await f.write(f"# #{channel.name} — {channel.guild.name}\n")
        await f.write(f"# Exported {fmt_timestamp(datetime.now(timezone.utc))}\n\n")

        try:
            async for msg in channel.history(limit=None, oldest_first=True):
                count += 1
                ts = fmt_timestamp(msg.created_at)
                await f.write(f"[{ts}] {msg.author}: {msg.content or ''}\n")

                for att in msg.attachments:
                    ext  = Path(att.filename).suffix
                    dest = attach_dir / f"{msg.id}_{att.id}{ext}"
                    rel  = dest.relative_to(out_dir)
                    await f.write(f"  [attachment: {att.filename} -> {rel}]\n")
                    await download_attachment(session, att.url, dest)
        except discord.Forbidden:
            await f.write("[No permission to read this channel]\n")

    return count


# ---------------------------------------------------------------------------
# Main client
# ---------------------------------------------------------------------------

class ExporterClient(discord.Client):
    def __init__(self, out_dir: Path, fmt: str,
                 guild_filter: int | None = None):
        intents = discord.Intents.default()
        intents.message_content = True
        super().__init__(intents=intents)
        self.out_dir      = out_dir
        self.fmt          = fmt
        self.guild_filter = guild_filter

    async def on_ready(self):
        print(f"Logged in as {self.user}")
        connector = aiohttp.TCPConnector(ssl=True)
        async with aiohttp.ClientSession(connector=connector) as session:
            await self._run_export(session)
        await self.close()

    async def _run_export(self, session: aiohttp.ClientSession):
        export_fn = (export_channel_html if self.fmt == 'html'
                     else export_channel_txt)
        total_msgs = 0
        total_ch   = 0

        guilds = [g for g in self.guilds
                  if self.guild_filter is None or g.id == self.guild_filter]

        for guild in guilds:
            guild_dir = self.out_dir / safe_name(guild.name)
            guild_dir.mkdir(parents=True, exist_ok=True)
            print(f"\nGuild: {guild.name} ({guild.id})")

            for channel in guild.channels:
                if not isinstance(channel, discord.TextChannel):
                    continue
                print(f"  Exporting #{channel.name} ...", end=' ', flush=True)
                try:
                    n = await export_fn(channel, guild_dir, session)
                    total_msgs += n
                    total_ch   += 1
                    print(f"{n} messages")
                except Exception as e:
                    print(f"ERROR: {e}")

        print(f"\nDone. {total_ch} channels, {total_msgs:,} messages total.")
        print(f"Output: {self.out_dir.resolve()}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Discord guild/channel exporter")
    parser.add_argument('--token',  required=True, help="Bot token")
    parser.add_argument('--format', choices=['html', 'txt'], default='html',
                        help="Export format (default: html)")
    parser.add_argument('--out',    default='./discord_export',
                        help="Output directory (default: ./discord_export)")
    parser.add_argument('--guild',  type=int, default=None,
                        help="Export only this guild ID (omit for all guilds)")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    client = ExporterClient(out_dir, args.format, args.guild)
    client.run(args.token)


if __name__ == '__main__':
    main()
