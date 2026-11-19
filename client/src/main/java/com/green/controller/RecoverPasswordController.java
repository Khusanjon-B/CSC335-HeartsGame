package com.green.controller;

import com.green.MessageFormatter;

import javafx.fxml.FXML;
import javafx.scene.control.TextField;
import javafx.scene.text.Text;

public class RecoverPasswordController extends Controller{
    @FXML
    private TextField emailField;
    @FXML
    private TextField pinField;
    @FXML 
    private Text errorMessage;


    @FXML
    private void requestPasswordRecovery(){
        if(emailField.getText().isEmpty()){
            errorMessage.setText("Please fill in the email field.");
            return;
        }
        if(pinField.getText().isEmpty()){
            errorMessage.setText("Please fill in the pin field.");
            return;
        }

        send(MessageFormatter.recoverPassword(emailField.getText(), pinField.getText()));
    }


    @FXML
    private void goToLogIn(){
        app.loadPage("login.fxml");
    }

}
