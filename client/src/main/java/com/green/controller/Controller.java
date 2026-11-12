package com.green.controller;

import com.green.*;


public class Controller {
    
    App app;
    public void setApp(App app) {
        this.app = app;
    }
    public void send(String str){
        MessageBridge.getOutQueue().offer(str);
    }
    public void dispose(){
        System.out.println("page disposed");
    }
}
