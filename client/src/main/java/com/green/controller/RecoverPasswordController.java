package com.green.controller;

import com.green.RequestManager;
import com.green.MessageFormatter;

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

        RequestManager.requestPasswordRecovery(
            usernameField.getText(),
            emailField.getText(),
            pinField.getText(),
            newPasswordField.getText(),
            response ->
                runOnUiThread(() -> {
                    if (responseSucceeded(response)) {
                        errorMessage.setText("Password recovery successful.");
                    } else {
                        errorMessage.setText(responseError(response, "Password recovery failed."));
                    }
                })
        );
    }


    @FXML
    private void goToLogIn(){
        app.loadPage("login.fxml");
    }

}
