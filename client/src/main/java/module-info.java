module client { 
    requires javafx.controls;
    requires javafx.fxml;
    requires transitive javafx.graphics;
    requires com.google.gson;

    exports com.green;
    exports com.green.controller;


    opens com.green.controller to javafx.fxml;
    opens com.green to javafx.fxml;
    
}
