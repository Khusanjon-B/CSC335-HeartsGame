package com.green.controller;

import com.green.MessageFormatter;
import com.green.RequestManager;

import javafx.fxml.FXML;
import javafx.application.Platform;
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
        RequestManager.sendRequest(
            MessageFormatter.login(usernameField.getText(), passwordField.getText()),
            response -> Platform.runLater(() -> {
                boolean success = response.has("success") && response.get("success").getAsBoolean();
                if (success) {
                    errorMessage.setText(response.has("message")
                        ? response.get("message").getAsString()
                        : "Login successful.");
                } else {
                    errorMessage.setText(response.has("message")
                        ? response.get("message").getAsString()
                        : "Login failed.");
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
