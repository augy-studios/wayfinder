# Discord Guild Exporter

## Does wayfinder apply here?

**No — and that's fine.** Discord's structure is a tree (guilds → categories →
channels → messages), not a Cartesian grid. Wayfinder is a grid-line traversal
primitive, not a graph traversal library. This tool uses standard Python async
iteration to walk the tree.

This is included as a standalone use-case alongside the wayfinder examples.

## What it does

- Logs in with a **bot token** using discord.py
- Iterates every text channel in every accessible guild (or a single guild)
- Downloads the full message history (oldest first)
- Downloads all file attachments
- Exports to **HTML** (styled, dark-theme, clickable links) or **TXT** (plain)

## Setup

```bash
pip install -r requirements.txt
```

### Create a bot

1. Go to [discord.com/developers/applications](https://discord.com/developers/applications)
2. New Application → Bot → Reset Token → copy the token
3. Enable **Message Content Intent** under Privileged Gateway Intents
4. Invite the bot to your server with permissions:
   - Read Messages / View Channels
   - Read Message History

Invite URL template (replace `BOT_CLIENT_ID`):
```
https://discord.com/api/oauth2/authorize?client_id=BOT_CLIENT_ID&permissions=68608&scope=bot
```

## Usage

```bash
# Export all guilds to HTML
python exporter.py --token YOUR_BOT_TOKEN --format html --out ./export

# Export all guilds to plain text
python exporter.py --token YOUR_BOT_TOKEN --format txt --out ./export

# Export a single guild only
python exporter.py --token YOUR_BOT_TOKEN --guild 123456789012345678 --format html --out ./export
```

## Output layout

```
export/
  My_Server/
    general.html          (or .txt)
    announcements.html
    attachments/
      general/
        <message_id>_<attachment_id>.png
        ...
  Another_Server/
    ...
```

## Notes

- Rate limits: discord.py handles rate limiting automatically.
- Large servers: exporting a server with millions of messages can take hours.
  The bot streams messages incrementally and never loads everything into RAM.
- Expired attachments: Discord CDN URLs can expire for old messages. The
  exporter silently skips attachments that return non-200.
