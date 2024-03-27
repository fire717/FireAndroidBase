package com.fireaicv.demo;

import static java.lang.Math.max;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.FileProvider;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.fireaicv.aimodel.AIModel;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;




public class MainActivity extends AppCompatActivity {
    private ImageView mImageView;
    private TextView mTextView;

    public AIModel my_model;

    String[] class_names = new String[]{
            "ceratium furca","eucampia","bacteriastrum","guinardia flaccida",
            "coscinodiscus","biddulphia","ditylum","dinophysis caudata",
            "ceratium fusus","chaetoceros","skeletonema","thalassionema frauenfeldii",
            "thalassionema nitzschioides","ceratium trichoceros","coscinodiscus flank","navicula",
            "pleurosigma pelagicum","helicotheca","protoperidinium","rhizosolenia",
            "ceratium tripos","corethron","detonula pumila","ceratium carriense"
    };
    Bitmap bitmap_in = null;
    Bitmap bitmap_out = null;

    private static final int REQUEST_IMAGE_CAPTURE = 1;
    private static final int REQUEST_CODE_PICK_IMAGE = 2;

    private String currentPhotoPath;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 获取相机权限
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CAMERA}, 1);
        }

        my_model = new AIModel(getAssets());

        mImageView = findViewById(R.id.image_show);
        mTextView = findViewById(R.id.text_result);
    }

    @Override
    protected void onStop() {
        super.onStop();

    }


    /*** 相机按钮 ***/
    public void clickCameraCrop(View view) {
        Intent takePictureIntent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        if (takePictureIntent.resolveActivity(getPackageManager()) != null) {
            File photoFile = null;
            try {
                photoFile = createImageFile();
            } catch (IOException ex) {
                // Error occurred while creating the File
                ex.printStackTrace();
            }
            if (photoFile != null) {
                Uri photoURI = FileProvider.getUriForFile(this,
                                                    "com.fireaicv.demo.fileprovider",
                                                            photoFile);
                takePictureIntent.putExtra(MediaStore.EXTRA_OUTPUT, photoURI);
                startActivityForResult(takePictureIntent, REQUEST_IMAGE_CAPTURE);
            }
        }
    }


    /*** 相册按钮 ***/
    public void clickGalleryCrop(View view) {
        Intent intent = new Intent(Intent.ACTION_PICK, MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
        startActivityForResult(intent, REQUEST_CODE_PICK_IMAGE);
    }

    /*** 运行按钮 ***/
    public void clickRunModel(View view) {
        // 模型推理
        if(bitmap_in != null) {
            Log.i("MainActivity", "predict button!");

            long startTime = System.currentTimeMillis();

            float[] res = my_model.detect(bitmap_in);

            long endTime = System.currentTimeMillis();
            long duration = endTime - startTime; // ms
            float fps = 1000.f/duration;
            Log.d("fps", "fps: "+fps);
            Log.d("runImgDetect", "res: "+res[0]+" "+res[1]+" "+res[2]+" "+res[3]+" "+res[4]+" "+res[5]+" "+res[6]);
            bitmap_out = bitmap_in.copy(Bitmap.Config.ARGB_8888, true);
            Log.d("runImgDetect", "bitmap_out h w: "+bitmap_out.getHeight()+" "+bitmap_out.getWidth());
            Canvas canvas = new Canvas(bitmap_out);

            Paint paint = new Paint();
            paint.setColor(Color.RED); // 设置点的颜色为红色
            paint.setStyle(Paint.Style.STROKE);
            int line_size = bitmap_out.getHeight()/200;
            paint.setStrokeWidth(line_size);

            Paint textPaint = new Paint();
            textPaint.setColor(Color.BLACK); // 设置文本颜色
            int text_size = bitmap_out.getHeight()/20;
            textPaint.setTextSize(text_size); // 设置文本大小
            textPaint.setColor(Color.RED);
            textPaint.setAntiAlias(true); // 设置抗锯齿

            int pid = 0;
            while(true){
                if(res[pid*6+5]==0){
                    break;
                }
                Rect rect = new Rect((int) res[pid*6+0], (int)res[pid*6+1], (int)res[pid*6+2], (int)res[pid*6+3]);
                canvas.drawRect(rect, paint);

                String text = class_names[(int)res[pid*6+4]]+" "+String.format("%.2f", res[pid*6+5]); // 要绘制的文本
                Log.d("fps", "pid: "+pid+" "+text);
                float textX =  max(0,res[pid*6+0]); // 文本的起始x坐标
                float textY = res[pid*6+1]-bitmap_out.getHeight()/20;
                if(textY<bitmap_out.getHeight()/20){
                    textY = res[pid*6+3]+bitmap_out.getHeight()/20;
                }
                canvas.drawText(text, textX, textY, textPaint);
                pid+=1;
            }

            mImageView.setImageBitmap(bitmap_out);

            String res_text = " FPS：" +String.valueOf(String.format("%.2f",fps));
            mTextView.setText(res_text);
        }else{
            Toast.makeText(MainActivity.this, "请先选择图片！", Toast.LENGTH_LONG).show();
        }

    }

    /*** 保存按钮 ***/
    public void clickSaveBitmap(View view) {
        // 从相机拍照并裁剪
        DateFormat format =new SimpleDateFormat("yyyyMMddHHmmss");
        if(bitmap_out!=null){
            saveBitmap(bitmap_out,format.format(new Date())+".jpg");
        }else{
            Toast.makeText(MainActivity.this, "图片为空", Toast.LENGTH_LONG).show();
        }

    }



    // Drawable转Bitmap的方法
    public Bitmap drawableToBitmap(Drawable drawable) {
        Bitmap bitmap = null;

        if (drawable instanceof BitmapDrawable) {
            BitmapDrawable bitmapDrawable = (BitmapDrawable) drawable;
            if(bitmapDrawable.getBitmap() != null) {
                return bitmapDrawable.getBitmap();
            }
        }

        if(drawable.getIntrinsicWidth() <= 0 || drawable.getIntrinsicHeight() <= 0) {
            bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        } else {
            bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        }

        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_IMAGE_CAPTURE && resultCode == RESULT_OK) {
            mImageView.setImageURI(Uri.parse(currentPhotoPath));
            Drawable drawable = mImageView.getDrawable();
            bitmap_in = drawableToBitmap(drawable);
        }
        if (requestCode == REQUEST_CODE_PICK_IMAGE && resultCode == RESULT_OK) {
            Uri uri = data.getData();
            bitmap_in = null;
            try {
                bitmap_in = BitmapFactory.decodeStream(getContentResolver().openInputStream(uri));
            } catch (FileNotFoundException e) {
                throw new RuntimeException(e);
            }
            mImageView.setImageBitmap(bitmap_in);
        }
    }



    // 创建图片文件保存
    private File createImageFile() throws IOException {
        // Create an image file name
        String timeStamp = new SimpleDateFormat("yyyyMMdd_HHmmss").format(new Date());
        String imageFileName = "JPEG_" + timeStamp + "_";
        File storageDir = getExternalFilesDir(Environment.DIRECTORY_PICTURES);
        File image = File.createTempFile(
                imageFileName,  /* prefix */
                ".jpg",         /* suffix */
                storageDir      /* directory */
        );
        // Save a file: path for use with ACTION_VIEW intents
        currentPhotoPath = image.getAbsolutePath();
        return image;
    }

    public static boolean isPathExist(String path) {
        File file = new File(path);
        if (file.exists()) {
            return true;
        }
        return false;
    }

    public void saveBitmap(Bitmap bitmap, String bitName){
        String fileName;
        File file;
        String dir_path;
        dir_path = Environment.getExternalStorageDirectory().getPath()+"/DCIM/Camera/";

        if(!isPathExist(dir_path)){
            dir_path = Environment.getExternalStorageDirectory().getPath()+"/DCIM/";
        }

        fileName = dir_path+bitName;

        file =new File(fileName);
        if(file.exists()){
            file.delete();
        }
        FileOutputStream out;
        try{
            out =new FileOutputStream(file);
            // 格式为 JPEG，照相机拍出的图片为JPEG格式的，PNG格式的不能显示在相册中
            if(bitmap.compress(Bitmap.CompressFormat.JPEG, 90, out)) {
                out.flush();
                out.close();
                // 插入图库
                MediaStore.Images.Media.insertImage(this.getContentResolver(), file.getAbsolutePath(), bitName, null);
            }
        }
        catch (FileNotFoundException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        // 发送广播，通知刷新图库的显示
        this.sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, Uri.parse("file://" + fileName)));

        Toast.makeText(MainActivity.this, "已保存到相册", Toast.LENGTH_LONG).show();
    }


}