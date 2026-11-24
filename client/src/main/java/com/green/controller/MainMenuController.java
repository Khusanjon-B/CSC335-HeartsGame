package com.green.controller;

import javafx.fxml.FXML;
import com.green.Player;
import com.green.RequestManager;

public class MainMenuController extends Controller {

    @FXML
    private void browseLobbies() {
        app.loadPage("lobbyBrowser.fxml");
    }

    @FXML
    private void logout() {
        RequestManager.requestLogout(response ->
            runOnUiThread(() -> {
                if (responseSucceeded(response)
                        || (response.has("error")
                            && response.get("error").isJsonPrimitive()
                            && response.getAsJsonPrimitive("error").isString()
                            && "NOT_LOGGED_IN".equals(response.get("error").getAsString()))) {
                    Player.resetPlayer();
                    app.loadPage("login.fxml");
                } else {
                    System.out.println(responseError(response, "Logout failed."));
                }
            })
        );
    }
}
