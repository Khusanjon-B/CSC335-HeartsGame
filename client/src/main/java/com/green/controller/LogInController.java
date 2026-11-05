package com.green.controller;


import javafx.fxml.FXML;

public class LogInController extends Controller{

    @FXML 
    private void attemptLogIn(){
        System.out.println("howdy");
        app.send("hi");
    }

}
