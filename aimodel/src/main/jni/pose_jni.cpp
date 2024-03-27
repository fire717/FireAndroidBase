// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

#include <android/log.h>

#include <jni.h>

#include <string>
#include <vector>

#include <platform.h>
#include <benchmark.h>



#include "ndkcamera.h"
#include "yolov7.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

static Yolo* g_yolov7 = 0;
static ncnn::Mutex lock;


static int draw_unsupported(cv::Mat& rgb){
    const char text[] = "unsupported";

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseLine);

    int y = (rgb.rows - label_size.height) / 2;
    int x = (rgb.cols - label_size.width) / 2;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0));

    return 0;
}

static int draw_fps(cv::Mat& rgb){
    // resolve moving average
    float avg_fps = 0.f;
    {
        static double t0 = 0.f;
        static float fps_history[10] = {0.f};

        double t1 = ncnn::get_current_time();
        if (t0 == 0.f)
        {
            t0 = t1;
            return 0;
        }

        float fps = 1000.f / (t1 - t0);
        t0 = t1;

        for (int i = 9; i >= 1; i--)
        {
            fps_history[i] = fps_history[i - 1];
        }
        fps_history[0] = fps;

        if (fps_history[9] == 0.f)
        {
            return 0;
        }

        for (int i = 0; i < 10; i++)
        {
            avg_fps += fps_history[i];
        }
        avg_fps /= 10.f;
    }

    char text[32];
    sprintf(text, "FPS=%.2f", avg_fps);

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    int y = 0;
    int x = rgb.cols - label_size.width;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));

    return 0;
}


class MyNdkCamera :
        public NdkCameraWindow {
public:
    virtual void on_image_render(cv::Mat &rgb) const;

    virtual void draw(cv::Mat &rgb, const std::vector<Object> &objects) const;

private:
    float bone_threshold = 0.5f;
};


void MyNdkCamera::draw(cv::Mat &rgb,
                       const std::vector<Object> &objects) const {



    for (int p_id = 0; p_id < objects.size(); p_id++) {

        cv::Scalar cc(0, 255, 0);
        cv::rectangle(rgb, objects[p_id].rect, cc, 2);


    }

}

void MyNdkCamera::on_image_render(cv::Mat &rgb) const {
    // nanodet  检测&绘图
//    cv::Mat test = cv::imread("/sdcard/ai/resized_img.png");
    {
        ncnn::MutexLockGuard g(lock);

        if (g_yolov7) {
            std::vector<Object> objects;
            g_yolov7->detect(rgb, objects);

            this->draw(rgb, objects);

        } else {
            draw_unsupported(rgb);
        }
    }

    draw_fps(rgb);
}



static MyNdkCamera* g_camera = 0;

extern "C" {

// public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);
JNIEXPORT jint JNICALL
Java_com_fireaicv_aimodel_AIModel_init(JNIEnv *env, jobject thiz, jobject assetManager,
                                           jint modelid, jint cpugpu) {
    if (modelid < 0 || modelid > 6 || cpugpu < 0 || cpugpu > 1) {
        return -1;
    }

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

    __android_log_print(ANDROID_LOG_DEBUG, "humanpose", "loadModel");

    bool use_gpu = (int) cpugpu == 1;

    // reload
    {
        ncnn::MutexLockGuard g(lock);

        if (use_gpu && ncnn::get_gpu_count() == 0) {
            // no gpu
            delete g_yolov7;
            g_yolov7 = 0;
        } else {
            if (!g_yolov7)
                g_yolov7 = new Yolo;
            const char* modeltypes[] =
                    {
                            "yolov7-tiny",
                    };
            const int target_sizes[] =
                    {
                            640,
                    };
            const float norm_vals[][3] =
                    {
                            {1 / 255.f, 1 / 255.f , 1 / 255.f},
                    };
            modelid = 0;
            const char* modeltype = modeltypes[(int)modelid];
            int target_size = target_sizes[(int)modelid];
            g_yolov7->load(mgr, modeltype, target_size, norm_vals[(int)modelid],use_gpu);
        }
    }

    return 0;
}

JNIEXPORT jint JNICALL
Java_com_fireaicv_aimodel_AIModel_deinit(JNIEnv *env, jobject thiz, jobject mgr) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnUnload");

    {
        ncnn::MutexLockGuard g(lock);

        delete g_yolov7;
        g_yolov7 = 0;

    }

    delete g_camera;
    g_camera = 0;

    return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved){
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnLoad");

    g_camera = new MyNdkCamera;

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved){
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnUnload");

    {
        ncnn::MutexLockGuard g(lock);

        delete g_yolov7;
        g_yolov7 = 0;

    }

    delete g_camera;
    g_camera = 0;
}



void ConvertBitmapToMat(JNIEnv* env, jobject bitmap, cv::Mat& dst) {
    AndroidBitmapInfo info;
    void* pixels = nullptr;

    // 获取 Bitmap 信息
    CV_Assert(AndroidBitmap_getInfo(env, bitmap, &info) >= 0);

    // 锁定 Bitmap 像素
    CV_Assert(AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0);

    // 转换像素格式
    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        // RGBA_8888 -> BGRA
        cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
        cv::cvtColor(tmp, dst, cv::COLOR_RGBA2BGR);
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        // RGB_565 -> BGR
        cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
        cv::cvtColor(tmp, dst, cv::COLOR_BGR5652BGR);
    } else {
        // 不支持的格式
        __android_log_print(ANDROID_LOG_DEBUG, "ConvertBitmapToMat", " Unsupported bitmap format");
    }

    // 解锁 Bitmap 像素
    AndroidBitmap_unlockPixels(env, bitmap);
}

JNIEXPORT JNICALL jfloatArray Java_com_fireaicv_aimodel_AIModel_detectFromBitmap(JNIEnv *env, jobject thiz, jobject imageSource) {


    cv::Mat input_mat_rgb;
//    ConvertBitmapToRGBAMat(env, imageSource, input_mat_rgb,  false);
    ConvertBitmapToMat(env,imageSource, input_mat_rgb );
//    AndroidBitmap_unlockPixels(env, imageSource);
//    cv::imwrite("/sdcard/ai/bitmap.jpg", input_mat_rgb);
//    cv::Mat input_mat_rgb2 = cv::imread("/sdcard/ai/bitmap.jpg");
//    cv::Mat input_mat_rgb2 = input_mat_rgb.clone();
    __android_log_print(ANDROID_LOG_DEBUG, "classify_model::detect0", " input mat: %d %d",input_mat_rgb.cols,input_mat_rgb.rows);



    jfloatArray result_array;
    result_array = env->NewFloatArray(6*100);
    jfloat *p_res_array = env->GetFloatArrayElements(result_array, NULL);

    if (g_yolov7) {
        ncnn::MutexLockGuard g(lock);
        std::vector<Object> objects;
        g_yolov7->detect(input_mat_rgb, objects);
        if(objects.size()>0){
            __android_log_print(ANDROID_LOG_DEBUG, "MyNdkCamera::on_image_render",
                                "persons.size %d", objects.size());

            for(int pid=0;pid<objects.size();pid++){
            int bw = objects[pid].rect.width;
            int bh = objects[pid].rect.height;

            int x0 = objects[pid].rect.x;
            int y0 = objects[pid].rect.y;
            int label = objects[pid].label;
            float score = objects[pid].prob;


            p_res_array[pid*6+0] = x0;
            p_res_array[pid*6+1] = y0;
            p_res_array[pid*6+2] = x0+bw;
            p_res_array[pid*6+3] = y0+bh;
            p_res_array[pid*6+4] = label;
            p_res_array[pid*6+5] = score;

            }
        }
//        std::vector<Person> persons;
//        g_nanodet->detect(input_mat_rgb, persons);
//        if(persons.size()>0){
//            __android_log_print(ANDROID_LOG_DEBUG, "MyNdkCamera::on_image_render",
//                                "persons.size %d", persons.size());
//
//            for(int pid=0;pid<persons.size();pid++){
//            int bw = persons[pid].rect.width;
//            int bh = persons[pid].rect.height;
//
//            int x0 = persons[pid].rect.x;
//            int y0 = persons[pid].rect.y;
//            int label = persons[pid].label;
//            float score = persons[pid].prob;
//
//
//            p_res_array[pid*6+0] = x0;
//            p_res_array[pid*6+1] = y0;
//            p_res_array[pid*6+2] = x0+bw;
//            p_res_array[pid*6+3] = y0+bh;
//            p_res_array[pid*6+4] = label;
//            p_res_array[pid*6+5] = score;
//
//            }
//        }
    }else{
        __android_log_print(ANDROID_LOG_DEBUG, "classify_model::detect0", " g_rtmpose not init.");
    }

    env->ReleaseFloatArrayElements(result_array, p_res_array, 0);
    return result_array;
}




// public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);
JNIEXPORT jboolean JNICALL Java_com_fireaicv_aimodel_AIModel_loadModel(JNIEnv* env, jobject thiz, jobject assetManager, jint modelid, jint cpugpu){
    if (modelid < 0 || modelid > 6 || cpugpu < 0 || cpugpu > 1){
        return JNI_FALSE;
    }

    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "loadModel %p", mgr);

    bool use_gpu = (int)cpugpu == 1;

    // reload
    {
        ncnn::MutexLockGuard g(lock);

        if (use_gpu && ncnn::get_gpu_count() == 0)
        {
            // no gpu
            delete g_yolov7;
            g_yolov7 = 0;
        }
        else
        {
            if (!g_yolov7)
                g_yolov7 = new Yolo;
            const char* modeltypes[] =
                    {
                            "yolov7-tiny",
                    };
            const int target_sizes[] =
                    {
                            640,
                    };
            const float norm_vals[][3] =
                    {
                            {1 / 255.f, 1 / 255.f , 1 / 255.f},
                    };
            modelid = 0;
            const char* modeltype = modeltypes[(int)modelid];
            int target_size = target_sizes[(int)modelid];
            g_yolov7->load(mgr, modeltype, target_size, norm_vals[(int)modelid],use_gpu);

        }
    }

    return JNI_TRUE;
}

// public native boolean openCamera(int facing);
JNIEXPORT jboolean JNICALL Java_com_fireaicv_aimodel_AIModel_openCamera(JNIEnv* env, jobject thiz, jint facing){
    if (facing < 0 || facing > 1)
        return JNI_FALSE;

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "openCamera %d", facing);

    g_camera->open((int)facing);

    return JNI_TRUE;
}

// public native boolean closeCamera();
JNIEXPORT jboolean JNICALL Java_com_fireaicv_aimodel_AIModel_closeCamera(JNIEnv* env, jobject thiz){
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "closeCamera");

    g_camera->close();

    return JNI_TRUE;
}

// public native boolean setOutputWindow(Surface surface);
JNIEXPORT jboolean JNICALL Java_com_fireaicv_aimodel_AIModel_setOutputWindow(JNIEnv* env, jobject thiz, jobject surface){
    ANativeWindow* win = ANativeWindow_fromSurface(env, surface);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "setOutputWindow %p", win);

    g_camera->set_window(win);

    return JNI_TRUE;
}

}
