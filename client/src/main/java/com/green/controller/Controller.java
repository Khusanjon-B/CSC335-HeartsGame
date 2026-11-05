package com.green.controller;

import com.green.App;

public class Controller {
    App app;
    public void setApp(App app) {
        this.app = app;
    }
    public void dispose(){
        System.out.println("page disposed");
    }

}
