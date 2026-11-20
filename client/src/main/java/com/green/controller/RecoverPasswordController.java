package com.green.controller;

import com.green.MessageFormatter;
import com.green.RequestManager;

import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.scene.control.TextField;
import javafx.scene.text.Text;

public class RecoverPasswordController extends Controller{
    @FXML
    private TextField usernameField;
    @FXML
    private TextField emailField;
    @FXML
    private TextField pinField;
    @FXML
    private TextField newPasswordField;
    @FXML 
    private Text errorMessage;


    @FXML
    private void requestPasswordRecovery(){
        if(usernameField.getText().isEmpty() || emailField.getText().isEmpty()
                || pinField.getText().isEmpty() || newPasswordField.getText().isEmpty()){
            errorMessage.setText("Please fill in all fields.");
            return;
        }

        if(!MessageFormatter.validPassword(newPasswordField.getText())){
            errorMessage.setText("Password does not meet the requirements.");
            return;
        }

        RequestManager.sendRequest(
            MessageFormatter.recoverPassword(
                usernameField.getText(), emailField.getText(), pinField.getText(), newPasswordField.getText()),
            response -> Platform.runLater(() -> {
                boolean success = response.has("success") && response.get("success").getAsBoolean();
                errorMessage.setText(response.has("message")
                    ? response.get("message").getAsString()
                    : (success ? "Password changed." : "Password recovery failed."));
            })
        );
    }


    @FXML
    private void goToLogIn(){
        app.loadPage("login.fxml");
    }

}
