package com.green.controller;

import com.green.MessageFormatter;

import javafx.fxml.FXML;
import javafx.scene.control.TextField;

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
    private void attemptSignUp(){
        System.out.println("Attempting to sign up with email: " + emailField.getText() + ", username: " + usernameField.getText() + ", password: " + passwordField.getText() + ", confirm password: " + confirmPasswordField.getText());
        if(emailField.getText().isEmpty() || usernameField.getText().isEmpty() || passwordField.getText().isEmpty() || confirmPasswordField.getText().isEmpty()){
            System.out.println("Please fill in all fields!");
            return;
        }
        if(!passwordField.getText().equals(confirmPasswordField.getText())){
            System.out.println("Passwords do not match!");
            return;
        }
        send(MessageFormatter.createAccount(emailField.getText(), usernameField.getText(), passwordField.getText()));
    }
}
