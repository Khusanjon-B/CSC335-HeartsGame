package com.green.controller;

import javafx.fxml.FXML;
import com.green.RequestManager;

public class MainMenuController extends Controller {

    @FXML
    private void logout() {
        RequestManager.requestLogout(response ->
            runOnUiThread(() -> {
                if (responseSucceeded(response)) {
                    app.loadPage("login.fxml");
                } else {
                    System.out.println(responseError(response, "Logout failed."));
                }
            })
        );
    }
}
