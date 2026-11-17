package com.green.controller;

import com.green.MessageFormatter;

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
        System.out.println("Attempting to log in with username: " + usernameField.getText() + " and password: " + passwordField.getText());
        send(MessageFormatter.login(usernameField.getText(), passwordField.getText()));
    }
    @FXML private void goToSignUp(){
        app.loadPage("createAccount.fxml");
    }
    @FXML private void goToForgotPassword(){
        app.loadPage("forgotPassword.fxml");
    }
    
}
