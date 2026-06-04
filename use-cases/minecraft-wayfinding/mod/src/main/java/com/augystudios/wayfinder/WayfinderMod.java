package com.augystudios.wayfinder;

import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.command.v2.CommandRegistrationCallback;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class WayfinderMod implements ModInitializer {

    public static final String MOD_ID = "wayfinder";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

    @Override
    public void onInitialize() {
        CommandRegistrationCallback.EVENT.register((dispatcher, registryAccess, environment) ->
            NavigateCommand.register(dispatcher));
        LOGGER.info("Wayfinder mod loaded.");
    }
}
