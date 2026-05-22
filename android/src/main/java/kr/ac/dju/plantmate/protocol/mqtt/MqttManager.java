package kr.ac.dju.plantmate.protocol.mqtt;

import android.os.Handler;
import android.os.Looper;

import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttAsyncClient;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

public class MqttManager {

    public interface ManagerListener {
        void onConnected(boolean reconnect);
        void onConnectionLost(String message);
        void onMessageReceived(String topic, String payload);
        void onError(String message);
    }

    public interface ActionCallback {
        void onSuccess();
        void onError(String message);
    }

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Map<String, Integer> subscriptions = new LinkedHashMap<>();
    private MqttAsyncClient client;
    private ManagerListener listener;

    public void setListener(ManagerListener listener) {
        this.listener = listener;
    }

    public boolean isConnected() {
        return client != null && client.isConnected();
    }

    public void connect(BrokerConfig config, ActionCallback callback) {
        try {
            if (client == null || !client.getServerURI().equals(config.getServerUri())) {
                if (client != null) {
                    client.disconnectForcibly();
                }
                client = new MqttAsyncClient(config.getServerUri(), config.getClientId(), new MemoryPersistence());
                client.setCallback(createCallback());
            }

            if (client.isConnected()) {
                postSuccess(callback);
                return;
            }

            MqttConnectOptions options = new MqttConnectOptions();
            options.setAutomaticReconnect(true);
            options.setCleanSession(true);
            options.setConnectionTimeout(10);
            options.setKeepAliveInterval(30);

            client.connect(options, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    postSuccess(callback);
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    postError(callback, messageOf(exception, "브로커 연결 실패"));
                }
            });
        } catch (MqttException exception) {
            postError(callback, messageOf(exception, "브로커 연결 실패"));
        }
    }

    public void disconnect() {
        if (client == null) {
            return;
        }
        try {
            if (client.isConnected()) {
                client.disconnect();
            }
        } catch (MqttException exception) {
            notifyError(messageOf(exception, "연결 종료 실패"));
        }
    }

    public void subscribe(String topic, int qos, ActionCallback callback) {
        if (!isConnected()) {
            postError(callback, "먼저 브로커에 연결하세요.");
            return;
        }
        try {
            subscriptions.put(topic, qos);
            client.subscribe(topic, qos, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    postSuccess(callback);
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    postError(callback, messageOf(exception, "토픽 구독 실패"));
                }
            });
        } catch (MqttException exception) {
            postError(callback, messageOf(exception, "토픽 구독 실패"));
        }
    }

    public void unsubscribe(String topic, ActionCallback callback) {
        if (!isConnected()) {
            postSuccess(callback);
            return;
        }
        try {
            subscriptions.remove(topic);
            client.unsubscribe(topic, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    postSuccess(callback);
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    postError(callback, messageOf(exception, "토픽 구독 해제 실패"));
                }
            });
        } catch (MqttException exception) {
            postError(callback, messageOf(exception, "토픽 구독 해제 실패"));
        }
    }

    public void publish(String topic, String payload, ActionCallback callback) {
        if (!isConnected()) {
            postError(callback, "먼저 브로커에 연결하세요.");
            return;
        }
        try {
            MqttMessage message = new MqttMessage(payload.getBytes(StandardCharsets.UTF_8));
            message.setQos(1);
            client.publish(topic, message, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    postSuccess(callback);
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    postError(callback, messageOf(exception, "메시지 발행 실패"));
                }
            });
        } catch (MqttException exception) {
            postError(callback, messageOf(exception, "메시지 발행 실패"));
        }
    }

    private MqttCallbackExtended createCallback() {
        return new MqttCallbackExtended() {
            @Override
            public void connectComplete(boolean reconnect, String serverURI) {
                mainHandler.post(() -> {
                    if (listener != null) {
                        listener.onConnected(reconnect);
                    }
                });
                if (reconnect) {
                    resubscribeAll();
                }
            }

            @Override
            public void connectionLost(Throwable cause) {
                mainHandler.post(() -> {
                    if (listener != null) {
                        listener.onConnectionLost(messageOf(cause, "브로커 연결 끊김"));
                    }
                });
            }

            @Override
            public void messageArrived(String topic, MqttMessage message) {
                String payload = new String(message.getPayload(), StandardCharsets.UTF_8);
                mainHandler.post(() -> {
                    if (listener != null) {
                        listener.onMessageReceived(topic, payload);
                    }
                });
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {
            }
        };
    }

    private void resubscribeAll() {
        if (!isConnected()) {
            return;
        }
        for (Map.Entry<String, Integer> entry : subscriptions.entrySet()) {
            try {
                client.subscribe(entry.getKey(), entry.getValue());
            } catch (MqttException exception) {
                notifyError(messageOf(exception, "재구독 실패"));
            }
        }
    }

    private void notifyError(String message) {
        mainHandler.post(() -> {
            if (listener != null) {
                listener.onError(message);
            }
        });
    }

    private void postSuccess(ActionCallback callback) {
        mainHandler.post(callback::onSuccess);
    }

    private void postError(ActionCallback callback, String message) {
        mainHandler.post(() -> callback.onError(message));
    }

    private String messageOf(Throwable throwable, String fallback) {
        if (throwable == null || throwable.getMessage() == null || throwable.getMessage().isEmpty()) {
            return fallback;
        }
        return throwable.getMessage();
    }
}
