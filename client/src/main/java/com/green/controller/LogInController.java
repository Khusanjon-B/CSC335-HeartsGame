package com.green.controller;

import com.green.MessageFormatter;

import javafx.fxml.FXML;
import javafx.scene.control.TextField;


public class LogInController extends Controller{
    @FXML
    private TextField usernameField;
    @FXML
    private TextField passwordField;
    @FXML 
    private void attemptLogIn(){
        System.out.println("Attempting to log in with username: " + usernameField.getText() + " and password: " + passwordField.getText());
        send(MessageFormatter.login(usernameField.getText(), passwordField.getText()));
    }
}
