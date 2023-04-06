package com.zhenxi.jnitrace.adapter;

import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;


import com.zhenxi.jnitrace.BuildConfig;
import com.zhenxi.jnitrace.R;
import com.zhenxi.jnitrace.bean.AppBean;

import com.zhenxi.jnitrace.utils.CLog;
import com.zhenxi.jnitrace.utils.Constants;
import com.zhenxi.jnitrace.utils.FileUtils;
import com.zhenxi.jnitrace.utils.GsonUtils;
import com.zhenxi.jnitrace.utils.RootUtils;
import com.zhenxi.jnitrace.utils.SpUtil;
import com.zhenxi.jnitrace.utils.ToastUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;


import static com.zhenxi.jnitrace.config.ConfigKey.CONFIG_JSON;
import static com.zhenxi.jnitrace.config.ConfigKey.FILTER_LIST;
import static com.zhenxi.jnitrace.config.ConfigKey.IS_LISTEN_TO_ALL;
import static com.zhenxi.jnitrace.config.ConfigKey.IS_SERIALIZATION;
import static com.zhenxi.jnitrace.config.ConfigKey.IS_SYSTEM_LOAD_INTO;
import static com.zhenxi.jnitrace.config.ConfigKey.JNITRACE_DEX_NAME;
import static com.zhenxi.jnitrace.config.ConfigKey.LIST_OF_FUNCTIONS;
import static com.zhenxi.jnitrace.config.ConfigKey.MOUDLE_SO_PATH;
import static com.zhenxi.jnitrace.config.ConfigKey.PACKAGE_NAME;
import static com.zhenxi.jnitrace.config.ConfigKey.SAVE_TIME;


import androidx.appcompat.app.AlertDialog;

import org.json.JSONObject;


/**
 * Created by lyh on 2019/2/14.
 */
public class MainListViewAdapter extends BaseAdapter {


    private ArrayList<AppBean> data;

    private final Context mContext;
    private final CheckBox isSerialization;

    private final CheckBox isSystemLoad;
    private AppBean mAppBean = null;

    public MainListViewAdapter(Context context,
                               ArrayList<AppBean> data,
                               CheckBox isSerialization,
                               CheckBox isSystemLoad) {
        this.mContext = context;
        this.data = data;
        this.isSerialization = isSerialization;
        this.isSystemLoad = isSystemLoad;
    }


    public void setData(ArrayList<AppBean> data) {

        this.data = data;

        notifyDataSetChanged();
    }


    @Override
    public int getCount() {
        return data == null ? 0 : data.size();
    }

    @Override
    public Object getItem(int position) {
        return data.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(final int position, View convertView, ViewGroup parent) {

        if (convertView == null) {
            convertView = View.inflate(mContext, R.layout.activity_list_item, null);
        }
        ViewHolder holder = ViewHolder.getHolder(convertView);
        AppBean appBean = data.get(position);

        holder.iv_appIcon.setImageBitmap(Constants.drawable2Bitmap(appBean.appIcon));
        holder.tv_appName.setText(appBean.appName);
        holder.tv_packageName.setText(appBean.packageName);
        holder.All.setOnClickListener(v ->
                initConfig(appBean)
        );
        return convertView;
    }

    private static final String[] items = {
            "JniTrace(JniEnv交互监听)",
            "libcString处理函数监听",
            "RegisterNative监听",
            "Linker加载SO监听",
            "监听全部Java方法调用(慎选,很容易造成程序卡顿)"
    };

    /**
     * 弹出对话框收集用户需要采集的So调用信息
     */
    private void showDialogForList(Context context) {
        View view = LayoutInflater.from(mContext).inflate(R.layout.dialog_input, null);
        EditText input = view.findViewById(R.id.ed_input);
        input.setVisibility(View.GONE);
        JSONObject jsonObject = new JSONObject();
        ArrayList<String> filtersList = new ArrayList<>();
        ArrayList<String> functionsList = new ArrayList<>();
        new AlertDialog.Builder(context)
                .setView(view)
                .setPositiveButton("确定", (dialog, which) -> {
                    String inputStr = input.getText().toString();
                    try {
                        if (inputStr.equals("ALL")) {
                            jsonObject.put(IS_LISTEN_TO_ALL, true);
                        } else {
                            jsonObject.put(IS_LISTEN_TO_ALL, false);
                            if (input.length() >= 1) {
                                String[] split = inputStr.split("\\|");
                                CLog.e("input str msg -> " + Arrays.toString(split));
                                filtersList.addAll(Arrays.asList(split));
                                if (!isSerialization.isChecked() && filtersList.size() >= 1) {
                                    String listJsonStr = GsonUtils.obj2str(filtersList);
                                    CLog.e("filter list json -> " + listJsonStr);
                                    jsonObject.put(FILTER_LIST, listJsonStr);
                                }
                            }
                        }
                        String functionsStr = GsonUtils.obj2str(functionsList);
                        CLog.e("functions list json -> " + functionsStr);
                        jsonObject.put(LIST_OF_FUNCTIONS, functionsStr);
                    } catch (Throwable e) {
                        CLog.e("put filter list error " + e);
                    }
                    //保存数据
                    saveConfig(mAppBean, jsonObject);
                    // 点击了确认按钮
                    dialog.dismiss();
                })
                .setNegativeButton("取消", (dialog, which) -> {
                    // 点击了取消按钮
                    dialog.dismiss();
                })
                .setMultiChoiceItems(items, null, (dialog, which, isChecked) -> {
                    if (isChecked) {
                        functionsList.add(which + "");
                        if (which == 0 || which == 1) {
                            input.setVisibility(View.VISIBLE);
                        }
                    } else {
                        functionsList.remove(which + "");
                        if (which == 0 || which == 1) {
                            input.setVisibility(View.GONE);
                        }
                    }
                    CLog.e("functions list " + functionsList);
                })
                .create()
                .show();
    }

    /**
     * 保存配置信息
     */
    public void initConfig(AppBean bean) {
        mAppBean = bean;
        if (!isSerialization.isChecked()) {
            //没有选中内存漫游,则执行正常逻辑,在弹窗里面进行保存
            showDialogForList(mContext);
            return;
        }
        saveConfig(bean, null);
    }

    private void saveConfig(AppBean bean, JSONObject jsonObject) {
        if (jsonObject == null) {
            jsonObject = new JSONObject();
        }
        try {
            jsonObject.put(PACKAGE_NAME, bean.packageName);
            jsonObject.put(IS_SYSTEM_LOAD_INTO, isSystemLoad.isChecked());

            try {
                PackageInfo packageInfo =
                        mContext.getPackageManager().getPackageInfo(BuildConfig.APPLICATION_ID, 0);
                jsonObject.put(MOUDLE_SO_PATH, packageInfo.applicationInfo.publicSourceDir);
            } catch (Throwable ignored) {
                jsonObject.put(MOUDLE_SO_PATH, null);
            }
            //保存时间,增加时效性
            jsonObject.put(SAVE_TIME, System.currentTimeMillis());
            jsonObject.put(IS_SERIALIZATION, isSerialization.isChecked());
        } catch (Throwable e) {
            CLog.e("save config to json error " + e);
        }
        saveConfigForLocation(bean, jsonObject);
    }

    private void saveConfigForLocation(AppBean bean, JSONObject jsonObject) {
        SpUtil.putString(mContext, CONFIG_JSON, jsonObject.toString());
        initConfig(bean.packageName, jsonObject);
        CLog.e("save config file info -> " + jsonObject);
        ToastUtils.showToast(mContext,
                "保存成功文件路径为\n" +
                        "data/data/" + bean.packageName);
    }

    /**
     * 很多加壳app
     * shared = new XSharedPreferences(BuildConfig.APPLICATION_ID, "config"); 导致失效
     * 通过root强行将数据保存一份到目标apk 私有目录
     */
    @SuppressWarnings("All")
    private void initConfig(String packageName, JSONObject jsonObject) {
        try {
            if (isSystemLoad.isChecked()) {
                String intoLib = null;
                ApplicationInfo my_apk_info = mContext.getApplicationInfo();
                intoLib = my_apk_info.nativeLibraryDir + "/lib" + BuildConfig.project_name + ".so";
                CLog.i("systemload into so path -> " + intoLib);
                try {
                    PackageManager packageManager = mContext.getPackageManager();
                    ApplicationInfo tag_apk_info = packageManager.getApplicationInfo(packageName, 0);
                    String libDir = tag_apk_info.nativeLibraryDir;
                    CLog.i("systemload load so path -> " + libDir);
                    RootUtils.execShell("mv -f " + intoLib + " " + libDir);

                    CLog.e("systemload mv success  !!!");
                } catch (Throwable e) {
                    CLog.e("get tag getApplicationInfo error " + e);
                }
            }

            File jnitraceModleData
                    = new File("/data/data/" + BuildConfig.APPLICATION_ID);

            File config = new File(jnitraceModleData, BuildConfig.project_name + "Config");
            CLog.e("temp config file path " + config.getPath());
            if (config.exists()) {
                boolean delete = config.delete();
                if (!delete) {
                    CLog.e("delete org config file error ,start root delete " + config.getPath());
                    RootUtils.execShell("rm -f " + config.getPath());
                }
            }
            FileUtils.makeSureDirExist(config.getParentFile());
            boolean configNewFile = config.createNewFile();
            if (!configNewFile) {
                CLog.e(">>>>>>>>>>> create temp config file error " + config.getPath());
                return;
            }
            config.setExecutable(true, false);
            config.setReadable(true, false);
            config.setWritable(true, false);
            CLog.e("start save config file " + config.getPath());
            FileUtils.saveString(config, jsonObject.toString());

            File temp = new File("/data/data/" + packageName);
            File tagConfigFile = new File(temp, BuildConfig.project_name + "Config");
            if (tagConfigFile.exists()) {
                RootUtils.execShell("rm -f " + tagConfigFile.getPath());
                CLog.i(">>>>>>>> initConfig rm -f finish  " + tagConfigFile.getPath());
            }
            File tagDexFile = new File(temp, JNITRACE_DEX_NAME);
            if (tagDexFile.exists()) {
                RootUtils.execShell("rm -f " + tagDexFile.getPath());
                CLog.i(">>>>>>>> initConfig rm -f finish  " + tagDexFile.getPath());
            }

            CLog.i(">>>>>>>>> start mv " +
                    "file " + config.getPath() + "->" + temp);
            //强制覆盖
            RootUtils.execShell("mv -f " + config.getPath() + " " + temp);

            //asset release
            File dexFile =
                    FileUtils.extractAssetFile(mContext, JNITRACE_DEX_NAME
                            , jnitraceModleData.getPath());
            if (!dexFile.exists()) {
                CLog.i(">>>>>>>> JnitraceDex.dex  release error " + dexFile.getPath());
                return;
            }

            RootUtils.execShell("mv -f " + dexFile.getPath() + " " + temp);

            //防止因为用户组权限问题导致open failed: EACCES (Permission denied)
            CLog.i(">>>>>>>>>> chmod 777 path -> " + tagConfigFile + " " + dexFile);
            RootUtils.execShell("chmod 777 " + tagConfigFile.getPath());
            RootUtils.execShell("chmod 777 " + tagDexFile.getPath());
        } catch (Throwable e) {
            CLog.e("initConfig error  " + e, e);
        }
    }


    private static class ViewHolder {
        TextView tv_appName, tv_packageName;
        LinearLayout All;
        ImageView iv_appIcon;

        ViewHolder(View convertView) {
            All = convertView.findViewById(R.id.ll_all);
            tv_packageName = convertView.findViewById(R.id.tv_packName);
            tv_appName = convertView.findViewById(R.id.tv_appName);
            iv_appIcon = convertView.findViewById(R.id.iv_appIcon);
        }

        static ViewHolder getHolder(View convertView) {
            ViewHolder holder = (ViewHolder) convertView.getTag();
            if (holder == null) {
                holder = new ViewHolder(convertView);
                convertView.setTag(holder);
            }
            return holder;
        }
    }
}
