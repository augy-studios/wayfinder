package com.augystudios.wayfinder;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import net.minecraft.particle.ParticleTypes;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.server.network.ServerPlayerEntity;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.text.Text;
import net.minecraft.util.math.BlockPos;

import java.util.List;

import static net.minecraft.server.command.CommandManager.argument;
import static net.minecraft.server.command.CommandManager.literal;

/**
 * Registers the /navigate command.
 *
 * /navigate <x> <y> <z>   — find and display path from your position to (x,y,z)
 * /navigate stop           — clear the displayed path (informational)
 * /navigate here           — print your current block coordinates
 */
public final class NavigateCommand {

    private NavigateCommand() {}

    public static void register(CommandDispatcher<ServerCommandSource> dispatcher) {
        dispatcher.register(literal("navigate")
            // /navigate <x> <y> <z>
            .then(argument("x", IntegerArgumentType.integer())
                .then(argument("y", IntegerArgumentType.integer(-64, 320))
                    .then(argument("z", IntegerArgumentType.integer())
                        .executes(ctx -> navigate(
                            ctx.getSource(),
                            IntegerArgumentType.getInteger(ctx, "x"),
                            IntegerArgumentType.getInteger(ctx, "y"),
                            IntegerArgumentType.getInteger(ctx, "z"))))))
            // /navigate stop
            .then(literal("stop")
                .executes(ctx -> stop(ctx.getSource())))
            // /navigate here
            .then(literal("here")
                .executes(ctx -> here(ctx.getSource())))
        );
    }

    // -------------------------------------------------------------------------

    private static int navigate(ServerCommandSource source, int x, int y, int z)
            throws com.mojang.brigadier.exceptions.CommandSyntaxException {
        ServerPlayerEntity player = source.getPlayerOrThrow();
        BlockPos start = player.getBlockPos();
        BlockPos goal  = new BlockPos(x, y, z);

        source.sendFeedback(() -> Text.literal(
            "§eWayfinder: searching " + fmt(start) + " → " + fmt(goal) + "…"), false);

        ThetaStar.Result result = ThetaStar.find(start, goal, player.getServerWorld());

        if (result.path().isEmpty()) {
            source.sendError(Text.literal("§cWayfinder: " + result.message()));
            return 0;
        }

        // Print waypoints to chat
        List<BlockPos> path = result.path();
        source.sendFeedback(() -> Text.literal("§a" + result.message()), false);
        for (int i = 0; i < path.size(); i++) {
            final int fi = i;
            source.sendFeedback(() -> Text.literal(
                "  §7[" + fi + "]§r " + fmt(path.get(fi))), false);
        }

        // Spawn END_ROD particles at each waypoint so you can see them in the world
        ServerWorld world = player.getServerWorld();
        for (BlockPos wp : path) {
            world.spawnParticles(player, ParticleTypes.END_ROD,
                true,
                wp.getX() + 0.5, wp.getY() + 1.0, wp.getZ() + 0.5,
                8, 0.15, 0.3, 0.15, 0.02);
        }

        return path.size();
    }

    private static int stop(ServerCommandSource source) {
        source.sendFeedback(() -> Text.literal("§7Wayfinder: path cleared."), false);
        return 1;
    }

    private static int here(ServerCommandSource source)
            throws com.mojang.brigadier.exceptions.CommandSyntaxException {
        ServerPlayerEntity player = source.getPlayerOrThrow();
        BlockPos pos = player.getBlockPos();
        source.sendFeedback(() -> Text.literal(
            "§eYour position: §r" + fmt(pos)), false);
        return 1;
    }

    private static String fmt(BlockPos p) {
        return "(" + p.getX() + ", " + p.getY() + ", " + p.getZ() + ")";
    }
}
