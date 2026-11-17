package com.green.controller;

import com.green.MessageFormatter;

import javafx.fxml.FXML;
import javafx.scene.control.TextField;
import javafx.scene.text.Text;

public class RecoverPasswordController extends Controller{
    @FXML
    private TextField emailField;
    @FXML 
    private Text errorMessage;


    @FXML
    private void requestPasswordRecovery(){
        if(emailField.getText().isEmpty()){
            errorMessage.setText("Please fill in the email field.");
            return;
        }
        String email = emailField.getText();

        send(MessageFormatter.recoverPassword(email));
    }


    @FXML
    private void goToLogIn(){
        app.loadPage("login.fxml");
    }

}
