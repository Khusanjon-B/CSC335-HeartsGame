package com.green.controller;

import com.google.gson.JsonObject;
import com.green.App;

import javafx.application.Platform;

public class Controller {
    
    App app;
    public void setApp(App app) {
        this.app = app;
    }
    public void dispose(){
        System.out.println("page disposed");
    }

    protected void runOnUiThread(Runnable action) {
        if (Platform.isFxApplicationThread()) {
            action.run();
        } else {
            Platform.runLater(action);
        }
    }

    protected boolean responseSucceeded(JsonObject response) {
        return response.has("success")
                && response.get("success").isJsonPrimitive()
                && response.getAsJsonPrimitive("success").isBoolean()
                && response.get("success").getAsBoolean();
    }

    protected String responseError(JsonObject response, String fallback) {
        if (response.has("message")
                && response.get("message").isJsonPrimitive()
                && response.getAsJsonPrimitive("message").isString()) {
            String error = response.get("message").getAsString();
            if (!error.isBlank()) {
                return error;
            }
        }
        if (response.has("error")
                && response.get("error").isJsonPrimitive()
                && response.getAsJsonPrimitive("error").isString()) {
            String error = response.get("error").getAsString();
            if (!error.isBlank()) {
                return error;
            }
        }
        return fallback;
    }

    public static void clearLocalSession() {
        com.green.Player.resetPlayer();
    }
}
