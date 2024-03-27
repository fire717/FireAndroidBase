package com.fireaicv.aimodel;


import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.util.Log;
import android.view.Surface;

public class AIModel {
    static {
        System.loadLibrary("libaimodel");
    }
    private static final String TAG = "HumanPose";
    private int device_type_default = 0; //0 for cpu 1for gpu
    public AIModel(AssetManager mgr) {
        initJavaLayer(mgr,device_type_default);
    }

    private void initJavaLayer(AssetManager mgr,int device_type){

        // 初始化模型
        int ret = init(mgr, 0, device_type);
        Log.d(TAG, "Model init result: " + ret);

    }
    public float[] detect(Bitmap input_bitmap) {
        return detectFromBitmap(input_bitmap);
    }

    public Bitmap detect_show(Bitmap input_bitmap) {
        // 传入：原图和人脸框的相对坐标（取值0.-1.）
//        Log.d(TAG,"getAntiSpoof detect res1 :"+ x0+" "+ y0+" "+ x1+" "+ y1);

        return input_bitmap;
    }

    private native int init(AssetManager mgr, int modelid, int cpugpu);
    public native int deinit(AssetManager mgr);
    private native float[] detectFromBitmap(Bitmap bitmap);


    public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);
    public native boolean openCamera(int facing);
    public native boolean closeCamera();
    public native boolean setOutputWindow(Surface surface);

}