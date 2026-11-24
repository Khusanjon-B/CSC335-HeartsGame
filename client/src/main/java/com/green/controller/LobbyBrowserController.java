package com.green.controller;

import java.util.ArrayList;
import java.util.List;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.green.Player;
import com.green.RequestManager;

import javafx.beans.property.SimpleStringProperty;
import javafx.fxml.FXML;
import javafx.scene.control.Button;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.text.Text;

public class LobbyBrowserController extends Controller {
    @FXML
    private TableView<LobbyEntry> lobbyTable;
    @FXML
    private TableColumn<LobbyEntry, String> lobbyIdColumn;
    @FXML
    private TableColumn<LobbyEntry, String> hostColumn;
    @FXML
    private TableColumn<LobbyEntry, String> playersColumn;
    @FXML
    private Button refreshButton;
    @FXML
    private Button createButton;
    @FXML
    private Button joinButton;
    @FXML
    private Button menuButton;
    @FXML
    private Text statusText;

    private boolean requestInProgress;

    @FXML
    private void initialize() {
        lobbyIdColumn.setCellValueFactory(cell ->
            new SimpleStringProperty(Integer.toString(cell.getValue().lobbyId())));
        hostColumn.setCellValueFactory(cell ->
            new SimpleStringProperty(cell.getValue().host()));
        playersColumn.setCellValueFactory(cell ->
            new SimpleStringProperty(cell.getValue().playerCount() + "/4"));
        joinButton.setDisable(true);
        lobbyTable.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
            joinButton.setDisable(requestInProgress || selected == null || selected.playerCount() >= 4));
        refreshLobbies();
    }

    @FXML
    private void refreshLobbies() {
        if (requestInProgress) {
            return;
        }
        setRequestInProgress(true);
        statusText.setText("Loading open lobbies...");
        RequestManager.requestGetLobbies(response -> runOnUiThread(() -> {
            setRequestInProgress(false);
            if (!responseSucceeded(response)) {
                statusText.setText(responseError(response, "Could not load lobbies."));
                return;
            }

            JsonArray lobbies = response.getAsJsonArray("lobbies");
            List<LobbyEntry> entries = new ArrayList<>();
            for (JsonElement element : lobbies) {
                JsonObject lobby = element.getAsJsonObject();
                entries.add(new LobbyEntry(
                    lobby.get("lobbyId").getAsInt(),
                    lobby.get("host").getAsString(),
                    lobby.get("playerCount").getAsInt()));
            }
            lobbyTable.getItems().setAll(entries);
            statusText.setText(entries.isEmpty() ? "No open lobbies. Create one to get started." : "");
        }));
    }

    @FXML
    private void createLobby() {
        if (requestInProgress) {
            return;
        }
        setRequestInProgress(true);
        statusText.setText("Creating lobby...");
        RequestManager.requestStartLobby(response -> runOnUiThread(() -> {
            setRequestInProgress(false);
            if (responseSucceeded(response)) {
                enterLobby(response);
            } else {
                statusText.setText(responseError(response, "Could not create lobby."));
            }
        }));
    }

    @FXML
    private void joinSelectedLobby() {
        LobbyEntry selected = lobbyTable.getSelectionModel().getSelectedItem();
        if (requestInProgress || selected == null || selected.playerCount() >= 4) {
            return;
        }
        setRequestInProgress(true);
        statusText.setText("Joining lobby...");
        RequestManager.requestJoinLobby(selected.lobbyId(), response -> runOnUiThread(() -> {
            setRequestInProgress(false);
            if (responseSucceeded(response)) {
                enterLobby(response);
            } else {
                statusText.setText(responseError(response, "Could not join lobby."));
            }
        }));
    }

    @FXML
    private void returnToMenu() {
        app.loadPage("mainMenu.fxml");
    }

    private void enterLobby(JsonObject response) {
        JsonArray responsePlayers = response.getAsJsonArray("players");
        List<String> players = new ArrayList<>();
        for (JsonElement player : responsePlayers) {
            players.add(player.getAsString());
        }
        Player.getCurrentPlayer().joinGame(
            response.get("playerNum").getAsInt(),
            response.get("lobbyId").getAsInt(),
            response.get("host").getAsString(),
            players);
        app.loadPage("lobby.fxml");
    }

    private void setRequestInProgress(boolean inProgress) {
        requestInProgress = inProgress;
        refreshButton.setDisable(inProgress);
        createButton.setDisable(inProgress);
        menuButton.setDisable(inProgress);
        LobbyEntry selected = lobbyTable.getSelectionModel().getSelectedItem();
        joinButton.setDisable(inProgress || selected == null || selected.playerCount() >= 4);
    }

    public record LobbyEntry(int lobbyId, String host, int playerCount) {
    }
}
