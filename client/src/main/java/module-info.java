module client { 
    requires javafx.controls;
    requires javafx.fxml;
    requires javafx.graphics;

    exports com.green;

    opens com.green to javafx.fxml;
    
}
