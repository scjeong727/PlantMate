package kr.ac.dju.plantmate;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import kr.ac.dju.plantmate.model.SessionState;
import kr.ac.dju.plantmate.protocol.ConnectionConfig;
import kr.ac.dju.plantmate.protocol.ProtocolType;
import kr.ac.dju.plantmate.service.PlantClientService;

public class LoginActivity extends AppCompatActivity {

    private static final String PREFS_NAME = "plantmate_prefs";
    private static final String KEY_LAST_HOST = "last_host";
    private static final String KEY_LAST_PROTOCOL = "last_protocol";
    private static final String KEY_LAST_CLIENT_ID = "last_client_id";
    private static final String DEFAULT_HOST = "192.168.0.34";
    private static final String DEFAULT_CLIENT_ID = "PlantMate-Android";

    private EditText editHost;
    private EditText editPort;
    private EditText editClientId;
    private EditText editLoginId;
    private EditText editLoginPw;
    private TextView textServerStatus;
    private RadioGroup protocolGroup;

    private PlantClientService service;
    private boolean bound;

    private final ServiceConnection connection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder binder) {
            PlantClientService.LocalBinder localBinder = (PlantClientService.LocalBinder) binder;
            service = localBinder.getService();
            bound = true;
            renderSession(service.getSessionState());
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            bound = false;
            service = null;
        }
    };

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_login);

        editHost = findViewById(R.id.edit_host);
        editPort = findViewById(R.id.edit_port);
        editClientId = findViewById(R.id.edit_client_id);
        editLoginId = findViewById(R.id.edit_login_id);
        editLoginPw = findViewById(R.id.edit_login_pw);
        textServerStatus = findViewById(R.id.text_server_status);
        protocolGroup = findViewById(R.id.protocol_group);

        editHost.setText(getLastHost());
        editClientId.setText(getLastClientId());
        applyProtocolSelection(getLastProtocol());
        protocolGroup.setOnCheckedChangeListener((group, checkedId) -> updateProtocolFields());
        updateProtocolFields();

        Intent intent = new Intent(this, kr.ac.dju.plantmate.service.PlantClientService.class);
        startService(intent);
        bindService(intent, connection, Context.BIND_AUTO_CREATE);

        ((Button) findViewById(R.id.btn_connect)).setOnClickListener(v -> connectServer());
        ((Button) findViewById(R.id.btn_login)).setOnClickListener(v -> login());
        ((Button) findViewById(R.id.btn_signup)).setOnClickListener(v -> signup());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (bound) {
            unbindService(connection);
            bound = false;
        }
    }

    private void connectServer() {
        if (service == null) {
            toast("서비스 연결 중입니다.");
            return;
        }

        String host = editHost.getText().toString().trim();
        int port;
        try {
            port = Integer.parseInt(editPort.getText().toString().trim());
        } catch (NumberFormatException e) {
            toast("포트를 확인하세요.");
            return;
        }

        ProtocolType protocolType = getSelectedProtocol();
        String clientId = editClientId.getText().toString().trim();

        service.connect(new ConnectionConfig(protocolType, host, port, clientId), new PlantClientService.ServiceCallback<SessionState>() {
            @Override
            public void onSuccess(SessionState data) {
                saveLastHost(host);
                saveLastProtocol(protocolType);
                saveLastClientId(clientId);
                renderSession(data);
                toast(protocolType == ProtocolType.MQTT ? "브로커 연결 완료" : "서버 연결 완료");
            }

            @Override
            public void onError(String message) {
                textServerStatus.setText(message);
                toast(message);
            }
        });
    }

    private void login() {
        if (service == null) {
            toast("서비스 연결 중입니다.");
            return;
        }

        service.login(
                editLoginId.getText().toString().trim(),
                editLoginPw.getText().toString().trim(),
                new PlantClientService.ServiceCallback<SessionState>() {
                    @Override
                    public void onSuccess(SessionState data) {
                        renderSession(data);
                        startActivity(new Intent(LoginActivity.this, MainActivity.class));
                        finish();
                    }

                    @Override
                    public void onError(String message) {
                        toast(message);
                    }
                }
        );
    }

    private void signup() {
        if (service == null) {
            toast("서비스 연결 중입니다.");
            return;
        }

        service.signup(
                editLoginId.getText().toString().trim(),
                editLoginPw.getText().toString().trim(),
                new PlantClientService.ServiceCallback<String>() {
                    @Override
                    public void onSuccess(String data) {
                        toast(data);
                    }

                    @Override
                    public void onError(String message) {
                        toast(message);
                    }
                }
        );
    }

    private void renderSession(SessionState state) {
        String status = state != null && state.isConnected() ? "연결됨" : "연결 안 됨";
        String protocol = state == null ? getSelectedProtocol().name() : state.getProtocolType().name();
        textServerStatus.setText(protocol + " / " + status);
    }

    private String getLastHost() {
        SharedPreferences preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        return preferences.getString(KEY_LAST_HOST, DEFAULT_HOST);
    }

    private void saveLastHost(String host) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putString(KEY_LAST_HOST, host)
                .apply();
    }

    private ProtocolType getLastProtocol() {
        String raw = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .getString(KEY_LAST_PROTOCOL, ProtocolType.TCP.name());
        try {
            return ProtocolType.valueOf(raw);
        } catch (IllegalArgumentException exception) {
            return ProtocolType.TCP;
        }
    }

    private void saveLastProtocol(ProtocolType protocolType) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putString(KEY_LAST_PROTOCOL, protocolType.name())
                .apply();
    }

    private String getLastClientId() {
        return getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .getString(KEY_LAST_CLIENT_ID, DEFAULT_CLIENT_ID);
    }

    private void saveLastClientId(String clientId) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putString(KEY_LAST_CLIENT_ID, clientId)
                .apply();
    }

    private void applyProtocolSelection(ProtocolType protocolType) {
        if (protocolType == ProtocolType.MQTT) {
            protocolGroup.check(R.id.radio_protocol_mqtt);
            return;
        }
        protocolGroup.check(R.id.radio_protocol_tcp);
    }

    private ProtocolType getSelectedProtocol() {
        return protocolGroup.getCheckedRadioButtonId() == R.id.radio_protocol_mqtt
                ? ProtocolType.MQTT
                : ProtocolType.TCP;
    }

    private void updateProtocolFields() {
        boolean isMqtt = getSelectedProtocol() == ProtocolType.MQTT;
        editClientId.setEnabled(isMqtt);
        editClientId.setAlpha(isMqtt ? 1f : 0.5f);
        String currentPort = editPort.getText().toString().trim();
        if (isMqtt && (currentPort.isEmpty() || "9000".equals(currentPort))) {
            editPort.setText("1883");
        } else if (!isMqtt && (currentPort.isEmpty() || "1883".equals(currentPort))) {
            editPort.setText("9000");
        }
    }

    private void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }
}
