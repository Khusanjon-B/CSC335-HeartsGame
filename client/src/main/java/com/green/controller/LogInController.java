package com.green.controller;

import com.green.RequestManager;
import com.green.Player;

import javafx.fxml.FXML;
import javafx.scene.control.TextField;
import javafx.scene.text.Text;


public class LogInController extends Controller{
    @FXML
    private TextField usernameField;
    @FXML
    private TextField passwordField;
    @FXML 
    private Text errorMessage;
    @FXML 
    private void attemptLogIn(){
        if(usernameField.getText().isEmpty() || passwordField.getText().isEmpty()){
            errorMessage.setText("Please fill in all fields.");
            return;
        }
        RequestManager.requestLogin(usernameField.getText(), passwordField.getText(), response ->
            runOnUiThread(() -> {
                if (responseSucceeded(response)) {
                    Player.setupPlayer(response.get("playerId").getAsInt(), response.get("username").getAsString());
                    app.loadPage("mainMenu.fxml");
                } else {
                    String message = response.has("message")
                        ? response.get("message").getAsString()
                        : "Login failed.";
                    if (response.has("failed_attempts")) {
                        int failedAttempts = response.get("failed_attempts").getAsInt();
                        message += " Attempt " + failedAttempts + " of 3.";
                        if (failedAttempts >= 3) {
                            message += " Account locked.";
                        }
                    }
                    errorMessage.setText(message);
                }
            })
        );
    }
    @FXML private void goToSignUp(){
        app.loadPage("createAccount.fxml");
    }
    @FXML private void goToForgotPassword(){
        app.loadPage("forgotPassword.fxml");
    }
    
}
