package com.green.controller;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.green.MessageHandler;
import com.green.Player;
import com.green.RequestManager;

import javafx.fxml.FXML;
import javafx.scene.control.Button;
import javafx.scene.control.ListView;
import javafx.scene.text.Text;

public class LobbyController extends Controller {
    @FXML
    private Text lobbyTitle;
    @FXML
    private Text lobbyStatus;
    @FXML
    private ListView<String> playerList;
    @FXML
    private Button leaveButton;

    private Player player;
    private boolean leaveInProgress;

    @Override
    public void setApp(com.green.App app) {
        super.setApp(app);
        player = Player.getCurrentPlayer();
        if (player == null || player.getLobbyId() == 0) {
            app.loadPage("lobbyBrowser.fxml");
            return;
        }
        renderPlayers();
        lobbyTitle.setText("Lobby " + player.getLobbyId()
            + " - hosted by " + player.getLobbyHost());
        lobbyStatus.setText("Waiting for players...");
        MessageHandler.setLobbyEventHandler(this::handleLobbyEvent);
    }

    @FXML
    private void leaveLobby() {
        if (leaveInProgress) {
            return;
        }
        leaveInProgress = true;
        leaveButton.setDisable(true);
        lobbyStatus.setText("Leaving lobby...");
        RequestManager.requestLeaveLobby(response -> runOnUiThread(() -> {
            if (responseSucceeded(response)
                    || (response.has("error")
                        && response.get("error").isJsonPrimitive()
                        && response.getAsJsonPrimitive("error").isString()
                        && "NOT_IN_LOBBY".equals(response.get("error").getAsString()))) {
                player.leaveGame();
                MessageHandler.setLobbyEventHandler(null);
                app.loadPage("lobbyBrowser.fxml");
            } else {
                leaveInProgress = false;
                leaveButton.setDisable(false);
                lobbyStatus.setText(responseError(response, "Could not leave lobby."));
            }
        }));
    }

    private void handleLobbyEvent(JsonObject event) {
        if (!event.has("lobbyId") || event.get("lobbyId").getAsInt() != player.getLobbyId()) {
            return;
        }
        runOnUiThread(() -> {
            String command = event.get("command").getAsString();
            if ("host_left".equals(command)) {
                player.leaveGame();
                MessageHandler.setLobbyEventHandler(null);
                app.loadPage("lobbyBrowser.fxml");
                return;
            }
            if (event.has("players")) {
                JsonArray names = event.getAsJsonArray("players");
                java.util.List<String> players = new java.util.ArrayList<>();
                for (int i = 0; i < names.size(); i++) {
                    players.add(names.get(i).getAsString());
                }
                player.setLobbyPlayers(players);
                renderPlayers();
            }
            if ("game_started".equals(command)) {
                lobbyStatus.setText("The game has started.");
            } else {
                lobbyStatus.setText("Waiting for players...");
            }
        });
    }

    private void renderPlayers() {
        playerList.getItems().setAll(player.getLobbyPlayers());
    }

    @Override
    public void dispose() {
        MessageHandler.setLobbyEventHandler(null);
    }
}
