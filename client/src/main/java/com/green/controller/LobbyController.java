package com.green.controller;

import java.util.ArrayList;
import java.util.List;

import com.google.gson.JsonObject;
import com.green.LobbyJson;
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
    @FXML
    private Button startGameButton;

    private Player player;
    private boolean leaveInProgress;
    private boolean startInProgress;
    private boolean gamePageLoaded;

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
        updateLobbyControls();
        MessageHandler.setLobbyEventHandler(this::handleLobbyEvent);
    }

    @FXML
    private void leaveLobby() {
        if (leaveInProgress || startInProgress) {
            return;
        }
        leaveInProgress = true;
        updateLobbyControls();
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
                updateLobbyControls();
                lobbyStatus.setText(responseError(response, "Could not leave lobby."));
            }
        }));
    }

    @FXML
    private void startGame() {
        if (startInProgress || leaveInProgress || !player.isHost()
                || player.getLobbyPlayers().size() < 4) {
            return;
        }
        startInProgress = true;
        updateLobbyControls();
        lobbyStatus.setText("Starting game...");
        RequestManager.requestStartGame(response -> runOnUiThread(() -> {
            if (responseSucceeded(response)) {
                showGamePage();
            } else {
                startInProgress = false;
                updateLobbyControls();
                lobbyStatus.setText(responseError(response, "Could not start game."));
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
                player.setLobbyPlayers(LobbyJson.playerNames(event.getAsJsonArray("players")));
                renderPlayers();
                updateLobbyControls();
            } else if ("player_joined".equals(command) && event.has("username")) {
                List<String> players = new ArrayList<>(player.getLobbyPlayers());
                String username = event.get("username").getAsString();
                if (!players.contains(username)) {
                    players.add(username);
                    player.setLobbyPlayers(players);
                    renderPlayers();
                    updateLobbyControls();
                }
            } else if ("player_left".equals(command) && event.has("username")) {
                List<String> players = new ArrayList<>(player.getLobbyPlayers());
                players.remove(event.get("username").getAsString());
                player.setLobbyPlayers(players);
                renderPlayers();
                updateLobbyControls();
            }
            if ("game_started".equals(command)) {
                showGamePage();
            } else {
                updateLobbyStatus();
            }
        });
    }

    private void updateLobbyControls() {
        boolean host = player.isHost();
        startGameButton.setVisible(host);
        startGameButton.setManaged(host);
        startGameButton.setDisable(startInProgress || leaveInProgress
            || player.getLobbyPlayers().size() < 4);
        leaveButton.setDisable(leaveInProgress || startInProgress);
        updateLobbyStatus();
    }

    private void updateLobbyStatus() {
        if (player.getLobbyPlayers().size() < 4) {
            lobbyStatus.setText("Waiting for players ("
                + player.getLobbyPlayers().size() + "/4)...");
        } else if (player.isHost()) {
            lobbyStatus.setText("Lobby is full. Start the game when ready.");
        } else {
            lobbyStatus.setText("Lobby is full. Waiting for the host to start.");
        }
    }

    private void showGamePage() {
        if (!gamePageLoaded) {
            gamePageLoaded = true;
            app.loadPage("game.fxml");
        }
    }

    private void renderPlayers() {
        playerList.getItems().setAll(player.getLobbyPlayers());
    }

    @Override
    public void dispose() {
        MessageHandler.setLobbyEventHandler(null);
    }
}
