package kr.ac.dju.plantmate;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import kr.ac.dju.plantmate.service.PlantClientService;
import kr.ac.dju.plantmate.ui.PlantFragment;

public class MainActivity extends AppCompatActivity {

    public interface ServiceAwareFragment {
        void onServiceReady();
    }

    private PlantClientService service;
    private boolean bound;
    private TextView textCurrentScreen;

    private final ServiceConnection connection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder binder) {
            PlantClientService.LocalBinder localBinder = (PlantClientService.LocalBinder) binder;
            service = localBinder.getService();
            bound = true;
            notifyCurrentFragmentServiceReady();
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
        setContentView(R.layout.activity_main);
        textCurrentScreen = findViewById(R.id.text_current_screen);

        bindService(new Intent(this, PlantClientService.class), connection, Context.BIND_AUTO_CREATE);

        if (savedInstanceState == null) {
            updateHeaderTitle();
            openFragment(new PlantFragment());
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (bound) {
            unbindService(connection);
            bound = false;
        }
    }

    public @Nullable PlantClientService getPlantService() {
        return service;
    }

    public boolean isServiceReady() {
        return service != null;
    }

    public void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    private void openFragment(Fragment fragment) {
        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.fragment_container, fragment)
                .commit();
        getSupportFragmentManager().executePendingTransactions();
        notifyCurrentFragmentServiceReady();
    }

    private void notifyCurrentFragmentServiceReady() {
        if (!isServiceReady()) {
            return;
        }

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment current = fragmentManager.findFragmentById(R.id.fragment_container);
        if (current instanceof ServiceAwareFragment) {
            ((ServiceAwareFragment) current).onServiceReady();
        }
    }

    private void updateHeaderTitle() {
        if (textCurrentScreen == null) {
            return;
        }
        textCurrentScreen.setText("식물");
    }
}
