package com.green;

import java.io.PrintWriter;
import java.net.ConnectException;
import java.net.Socket;

public class SocketManager implements Runnable{

    private String bridgeIp;
    private int port;

    public SocketManager(String bridgeIp, int port){
        this.bridgeIp = bridgeIp;
        this.port = port;
    }
    @Override
    public void run() {
        Socket socket = null;
        boolean retryConnection = true;
        while(retryConnection){
            System.out.println("Connecting to " + bridgeIp);
            try{
                socket = new Socket(bridgeIp, port);
                System.out.println("Connected to " + bridgeIp);
                PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
                Thread readerThread = new Thread(new Reader(socket));
                readerThread.setDaemon(true);
                readerThread.start();
                retryConnection = false;
                String message = "";
                while(socket!=null && !socket.isClosed() && out!=null){
                    try{
                        message = MessageBridge.getOutQueue().take();
                        out.println(message);
                        System.out.println("sending: " + message);
                    }catch(Exception e){
                        socket.close();
                        System.out.println("output stream connection ended");
                    }
                }
            }catch(ConnectException e){
                System.out.println("Connection timed out, retrying...");
            }catch(Exception e){
                retryConnection = false;
                System.out.println("Connection Failed");
                e.printStackTrace();
            }
        }
    }
    

}
