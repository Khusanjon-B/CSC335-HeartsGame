package com.green.controller;

import com.green.RequestManager;

import javafx.fxml.FXML;
import javafx.scene.control.TextField;
import javafx.scene.text.Text;

public class SignUpController extends Controller {
    
    @FXML
    private TextField emailField;
    @FXML
    private TextField usernameField;
    @FXML
    private TextField passwordField;
    @FXML
    private TextField confirmPasswordField;
    @FXML
    private TextField pinField;
    @FXML
    private Text errorMessage;
    @FXML 
    private void attemptSignUp(){
        if(emailField.getText().isEmpty() || usernameField.getText().isEmpty() || passwordField.getText().isEmpty() || confirmPasswordField.getText().isEmpty() || pinField.getText().isEmpty()){
            errorMessage.setText("Please fill in all fields!");
            return;
        }
        if(!passwordField.getText().equals(confirmPasswordField.getText())){
            errorMessage.setText("Passwords do not match!");
            return;
        }
        try {
            RequestManager.requestAccountCreation(
                    emailField.getText(),
                    usernameField.getText(),
                    passwordField.getText(),
                    pinField.getText(),
                    response -> runOnUiThread(() -> {
                        if (responseSucceeded(response)) {
                            app.loadPage("login.fxml");
                        } else {
                            errorMessage.setText(responseError(response, "Account creation failed."));
                        }
                    })
            );
        } catch (IllegalArgumentException e) {
            errorMessage.setText(e.getMessage());
        }
    }

    @FXML
    private void goToLogIn(){
        app.loadPage("login.fxml");
    }

}
