package kr.ac.dju.plantmate;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import kr.ac.dju.plantmate.config.AppConfig;
import kr.ac.dju.plantmate.model.SessionState;
import kr.ac.dju.plantmate.protocol.ConnectionConfig;
import kr.ac.dju.plantmate.protocol.ProtocolType;
import kr.ac.dju.plantmate.service.PlantClientService;

public class LoginActivity extends AppCompatActivity {

    private static final String PREFS_NAME = "plantmate_prefs";
    private static final String KEY_LAST_HOST = "last_host";

    private enum PendingAction {
        LOGIN,
        SIGNUP
    }

    private View authPanel;
    private View serverPanel;
    private EditText editHost;
    private EditText editLoginId;
    private EditText editLoginPw;
    private TextView textServerStatus;
    private TextView textServerTitle;
    private Button buttonServerProceed;

    private PlantClientService service;
    private boolean bound;
    private PendingAction pendingAction = PendingAction.LOGIN;
    private AppConfig appConfig;

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
        appConfig = AppConfig.load(this);

        authPanel = findViewById(R.id.auth_panel);
        serverPanel = findViewById(R.id.server_panel);
        editHost = findViewById(R.id.edit_host);
        editLoginId = findViewById(R.id.edit_login_id);
        editLoginPw = findViewById(R.id.edit_login_pw);
        textServerStatus = findViewById(R.id.text_server_status);
        textServerTitle = findViewById(R.id.text_server_title);
        buttonServerProceed = findViewById(R.id.btn_server_proceed);

        editHost.setText(getLastHost());

        Intent intent = new Intent(this, kr.ac.dju.plantmate.service.PlantClientService.class);
        startService(intent);
        bindService(intent, connection, Context.BIND_AUTO_CREATE);

        ((Button) findViewById(R.id.btn_login)).setOnClickListener(v -> showServerSetup(PendingAction.LOGIN));
        ((Button) findViewById(R.id.btn_signup)).setOnClickListener(v -> showServerSetup(PendingAction.SIGNUP));
        buttonServerProceed.setOnClickListener(v -> connectServerAndContinue());
        ((Button) findViewById(R.id.btn_server_back)).setOnClickListener(v -> showAuthPanel());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (bound) {
            unbindService(connection);
            bound = false;
        }
    }

    private void connectServerAndContinue() {
        if (service == null) {
            toast("서비스 연결 중입니다.");
            return;
        }

        String host = editHost.getText().toString().trim();
        if (host.isEmpty()) {
            toast("서버 IP를 입력하세요.");
            return;
        }

        service.connect(new ConnectionConfig(ProtocolType.MQTT, host, appConfig.getMqttPort(), appConfig.getClientId()), new PlantClientService.ServiceCallback<SessionState>() {
            @Override
            public void onSuccess(SessionState data) {
                saveLastHost(host);
                renderSession(data);
                continuePendingAction();
            }

            @Override
            public void onError(String message) {
                textServerStatus.setText(message);
                toast(message);
            }
        });
    }

    private void showServerSetup(PendingAction action) {
        if (!validateCredentials()) {
            return;
        }

        pendingAction = action;
        authPanel.setVisibility(View.GONE);
        serverPanel.setVisibility(View.VISIBLE);

        if (action == PendingAction.SIGNUP) {
            textServerTitle.setText("회원가입 서버 설정");
            buttonServerProceed.setText("서버 연결 후 회원가입");
        } else {
            textServerTitle.setText("로그인 서버 설정");
            buttonServerProceed.setText("서버 연결 후 로그인");
        }
    }

    private void showAuthPanel() {
        serverPanel.setVisibility(View.GONE);
        authPanel.setVisibility(View.VISIBLE);
    }

    private boolean validateCredentials() {
        String loginId = editLoginId.getText().toString().trim();
        String loginPw = editLoginPw.getText().toString().trim();
        if (loginId.isEmpty() || loginPw.isEmpty()) {
            toast("ID와 비밀번호를 입력하세요.");
            return false;
        }
        return true;
    }

    private void continuePendingAction() {
        if (pendingAction == PendingAction.SIGNUP) {
            signup();
            return;
        }
        login();
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
        String protocol = state == null ? ProtocolType.MQTT.name() : state.getProtocolType().name();
        textServerStatus.setText(protocol + " / " + status);
    }

    private String getLastHost() {
        SharedPreferences preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        return preferences.getString(KEY_LAST_HOST, appConfig.getDefaultHost());
    }

    private void saveLastHost(String host) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putString(KEY_LAST_HOST, host)
                .apply();
    }

    private void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }
}
