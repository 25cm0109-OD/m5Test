package jp.ac.jec.cm25xx.irhcker;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.TextView;

import java.util.List;

public class SignalAdapter extends BaseAdapter {

    public interface Listener {
        void onSend(Signal signal);
        void onEdit(Signal signal);
        void onFavorite(Signal signal);
    }

    private final List<Signal> signals;
    private final LayoutInflater inflater;
    private final Listener listener;

    public SignalAdapter(
            MainActivity activity,
            List<Signal> signals,
            Listener listener
    ) {
        this.signals = signals;
        this.inflater = LayoutInflater.from(activity);
        this.listener = listener;
    }

    @Override
    public int getCount() {
        return signals.size();
    }

    @Override
    public Signal getItem(int position) {
        return signals.get(position);
    }

    @Override
    public long getItemId(int position) {
        return signals.get(position).id;
    }

    @Override
    public View getView(int position, View oldView, ViewGroup parent) {
        View view = oldView;

        if (view == null) {
            view = inflater.inflate(R.layout.item_signal, parent, false);
        }

        Signal signal = getItem(position);

        TextView favoriteButton = view.findViewById(R.id.favoriteButton);
        TextView nameText = view.findViewById(R.id.signalNameText);
        TextView manufacturerText =
                view.findViewById(R.id.manufacturerText);
        TextView detailText = view.findViewById(R.id.signalDetailText);
        Button editButton = view.findViewById(R.id.editButton);

        favoriteButton.setText(signal.favorite ? "★" : "☆");
        nameText.setText(signal.getDisplayName());
        manufacturerText.setText(signal.getManufacturerDisplay());

        detailText.setText(
                signal.protocol + " / "
                        + signal.bits + "bit / "
                        + signal.deviceId
        );

        // 行を直接押したら送信
        view.setOnClickListener(v -> listener.onSend(signal));

        favoriteButton.setOnClickListener(
                v -> listener.onFavorite(signal)
        );

        editButton.setOnClickListener(
                v -> listener.onEdit(signal)
        );

        return view;
    }
}