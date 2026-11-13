package com.green;


import com.green.controller.Controller;


import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.layout.Pane;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

public class App extends Application{

    private int width = 800;
    private int height = 450;
    private Stage stage;


    private FXMLLoader loader;
    private Controller controller;
    private VBox FXMLpage;

    private Pane root;




    @Override
    public void start(Stage arg0) throws Exception {
        this.stage = new Stage();
        this.stage.setTitle("cards");
        root = new Pane();
        try{
            loader = new FXMLLoader(getClass().getResource("login.fxml"));
            FXMLpage = loader.load();
            controller = loader.getController();
            controller.setApp(this);
            root.getChildren().addAll(FXMLpage);
            stage.setScene(new Scene(root, width, height));
            stage.show();
        }catch(Exception e){
            e.printStackTrace();
        }
    }


    public void loadPage(String page){
        try{
            controller.dispose();

            loader = new FXMLLoader(getClass().getResource(page));
            FXMLpage = loader.load();
            controller = loader.getController();
            controller.setApp(this);
            root.getChildren().clear();
            root.getChildren().addAll(FXMLpage);
        }catch(Exception e){
            e.printStackTrace();
        }
        System.out.println(page);
    }
    @Override 
    public void stop(){
        System.exit(0);
    }

    public void send(String str){
            MessageBridge.getOutQueue().offer(str);
    }

}
