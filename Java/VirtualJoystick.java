package com.example.xemu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

public class VirtualJoystick extends View {

    private Paint outerCirclePaint;
    private Paint innerCirclePaint;
    private PointF center;
    private PointF joystickPosition;
    private float outerRadius;
    private float innerRadius;
    private JoystickListener joystickListener;
    private Bitmap outerCircleBitmap;
    private Bitmap innerCircleBitmap;

    public VirtualJoystick(Context context) {
        super(context);
        init();
    }

    public VirtualJoystick(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        outerCirclePaint = new Paint();
        outerCirclePaint.setColor(Color.GRAY);
        outerCirclePaint.setAlpha(100);

        innerCirclePaint = new Paint();
        innerCirclePaint.setColor(Color.BLUE);
        innerCirclePaint.setAlpha(150);

        center = new PointF();
        joystickPosition = new PointF();

        outerCircleBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.outer_circle);
        innerCircleBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.inner_circle);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        center.set(w / 2f, h / 2f);
        joystickPosition.set(center);
        outerRadius = Math.min(w, h) / 2f;
        innerRadius = outerRadius / 3f;

        outerCircleBitmap = Bitmap.createScaledBitmap(outerCircleBitmap, (int) (outerRadius * 2), (int) (outerRadius * 2), true);
        innerCircleBitmap = Bitmap.createScaledBitmap(innerCircleBitmap, (int) (innerRadius * 2), (int) (innerRadius * 2), true);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawBitmap(outerCircleBitmap, center.x - outerRadius, center.y - outerRadius, null);
        canvas.drawBitmap(innerCircleBitmap, joystickPosition.x - innerRadius, joystickPosition.y - innerRadius, null);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
                float x = event.getX();
                float y = event.getY();
                float distance = (float) Math.sqrt(Math.pow(x - center.x, 2) + Math.pow(y - center.y, 2));

                if (distance > outerRadius) {
                    x = center.x + (x - center.x) * outerRadius / distance;
                    y = center.y + (y - center.y) * outerRadius / distance;
                }

                joystickPosition.set(x, y);
                invalidate();

                if (joystickListener != null) {
                    float normalizedX = (x - center.x) / outerRadius;
                    float normalizedY = (y - center.y) / outerRadius;
                    joystickListener.onJoystickMoved(normalizedX, normalizedY);
                }
                break;

            case MotionEvent.ACTION_UP:
                joystickPosition.set(center);
                invalidate();

                if (joystickListener != null) {
                    joystickListener.onJoystickMoved(0, 0);
                }
                break;
        }
        return true;
    }

    public void setJoystickListener(JoystickListener listener) {
        this.joystickListener = listener;
    }

    public interface JoystickListener {
        void onJoystickMoved(float x, float y);
    }
}