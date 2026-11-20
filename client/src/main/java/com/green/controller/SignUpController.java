package com.green.controller;

import com.green.MessageFormatter;
import com.green.RequestManager;

import javafx.application.Platform;
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
            System.out.println("Please fill in all fields!");
            errorMessage.setText("Please fill in all fields!");
            return;
        }
        if(!passwordField.getText().equals(confirmPasswordField.getText())){
            System.out.println("Passwords do not match!");
            errorMessage.setText("Passwords do not match!");
            return;
        }
        if(!MessageFormatter.validPassword(passwordField.getText())){
            System.out.println("Password does not meet the requirements!");
            errorMessage.setText("Password does not meet the requirements!");
            return;
        }
        if(!MessageFormatter.validEmail(emailField.getText())){
            System.out.println("Invalid email format!");
            errorMessage.setText("Invalid email format!");
            return;
        }
        if(!MessageFormatter.validUsername(usernameField.getText())){
            System.out.println("Invalid username format!");
            errorMessage.setText("Invalid username format!");
            return;
        }
        if(!MessageFormatter.validPin(pinField.getText())){
            System.out.println("Invalid pin format!");
            errorMessage.setText("Invalid pin format!");
            return;
        }
        RequestManager.sendRequest(
            MessageFormatter.createAccount(emailField.getText(), usernameField.getText(),
                passwordField.getText(), pinField.getText()),
            response -> Platform.runLater(() -> {
                boolean success = response.has("success") && response.get("success").getAsBoolean();
                errorMessage.setText(response.has("message")
                    ? response.get("message").getAsString()
                    : (success ? "Account created. You can now log in." : "Account creation failed."));
            })
        );
    }

    @FXML
    private void goToLogIn(){
        app.loadPage("login.fxml");
    }

}
