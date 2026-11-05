module client { 
    requires javafx.controls;
    requires javafx.fxml;
    requires javafx.graphics;

    // 2. Allow JavaFX to access your package to boot the Application
    exports com.green;

    // 3. Open your package to javafx.fxml for reflective controller lookup
    opens com.green to javafx.fxml;
    
}
