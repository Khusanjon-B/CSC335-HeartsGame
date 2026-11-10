package com.green;

import javafx.application.Application;

public class Main {
    static String bridgeIp = "192.168.4.200"; 
    static int port = 5000;
    static boolean GUI = true;


    public static void main(String[] args) {


        if(GUI){
            Thread GUIThread = new Thread(() -> {
                Application.launch(App.class, args);
            });
            GUIThread.start();
        }else{

        }
        Thread socketThread = new Thread(new SocketManager(bridgeIp, port));
        socketThread.setDaemon(true);
        socketThread.start();

        MessageBridge.getQueue().offer("hello");
        
    }
}