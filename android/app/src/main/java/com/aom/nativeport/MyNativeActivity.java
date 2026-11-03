package com.aom.nativeport;



import android.Manifest;

import android.app.Dialog;

import android.content.pm.PackageManager;

import android.content.res.AssetManager;

import android.media.MediaPlayer;

import android.os.Bundle;

import android.util.Log;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.database.Cursor;

import android.view.Gravity;

import android.view.ViewGroup;

import android.view.Window;

import android.widget.Button;

import android.widget.FrameLayout;

import android.widget.VideoView;



import java.io.File;

import java.io.FileNotFoundException;

import java.io.FileOutputStream;

import java.io.IOException;

import java.io.InputStream;

import java.util.ArrayList;

import java.util.List;



public class MyNativeActivity extends android.app.NativeActivity {
    static {
        System.loadLibrary("c++_shared");
        System.loadLibrary("aom_native");
    }


    private static final String TAG = "AoM-Activity";

    private static final int REQUEST_STORAGE = 1001;

    private static final String[] INTRO_VIDEO_NAMES = {"aom-logo.mp4", "introcg.mp4"};

    private static final String[] TITANS_VIDEO_NAMES = {"aom-logo.mp4", "xintrocg.mp4"};

    private static final String[] ALL_KNOWN_VIDEOS = {

        "aom-logo.mp4",

        "introcg.mp4",

        "xintrocg.mp4",

        "outtakes.mp4",

        "credits.mp4",

        "xcredits.mp4"

    };



    private Dialog cinematicDialog;

    private FrameLayout cinematicOverlay;

    private VideoView cinematicVideo;

    private List<File> introVideos = new ArrayList<>();

    private int introVideoIndex;



    @Override

    protected void onCreate(Bundle savedInstanceState) {

        copyGameAssetsIfNeeded();

        super.onCreate(savedInstanceState);

        playIntroCinematics();

        if (!hasStoragePermission()) {
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                try {
                    Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + getPackageName()));
                    startActivity(intent);
                } catch (Exception e) {
                    try {
                        Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                        startActivity(intent);
                    } catch (Exception ignored) {}
                }
            } else {
                requestPermissions(
                        new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        REQUEST_STORAGE);
            }
        }

    }



    public void replayOpeningCinematics() {

        playCinematicPlaylist(INTRO_VIDEO_NAMES);

    }



    public void playTitansCinematics() {

        playCinematicPlaylist(TITANS_VIDEO_NAMES);

    }



    public void playOuttakes() {

        playCinematicPlaylist(new String[]{"outtakes.mp4"});

    }



    public void playCredits() {

        playCinematicPlaylist(new String[]{"credits.mp4"});

    }



    public void playTitansCredits() {

        playCinematicPlaylist(new String[]{"xcredits.mp4"});

    }



    public void playCinematicPlaylist(String[] videoNames) {

        runOnUiThread(() -> {

            finishIntroCinematics();

            introVideos.clear();

            introVideoIndex = 0;



            File externalDir = getExternalFilesDir(null);

            if (externalDir != null) {

                File aviDir = new File(new File(externalDir, "AoM"), "avi");

                for (String videoName : videoNames) {

                    File video = new File(aviDir, videoName);

                    if (video.isFile() && video.length() > 0) {

                        introVideos.add(video);

                    }

                }

            }

            if (introVideos.isEmpty()) {

                Log.w(TAG, "No video files found for requested cinematic playlist");

                return;

            }

            startCinematicDialog();

        });

    }



    private boolean hasStoragePermission() {
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            return android.os.Environment.isExternalStorageManager();
        }
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }



    @Override

    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {

        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == REQUEST_STORAGE

                && (grantResults.length == 0 || grantResults[0] != PackageManager.PERMISSION_GRANTED)) {

            Log.w(TAG, "Storage permission denied; original shared game files may be unavailable");

        }

    }



    private void copyGameAssetsIfNeeded() {

        File externalDir = getExternalFilesDir(null);

        if (externalDir == null) {

            Log.e(TAG, "App-specific external storage is unavailable");

            return;

        }



        File aomDir = new File(externalDir, "AoM");

        File texturesDir = new File(aomDir, "textures");

        File dataDir = new File(aomDir, "data");

        File aviDir = new File(aomDir, "avi");

        if (!texturesDir.mkdirs() && !texturesDir.isDirectory()) {

            Log.e(TAG, "Cannot create texture directory: " + texturesDir);

        }

        if (!dataDir.mkdirs() && !dataDir.isDirectory()) {

            Log.e(TAG, "Cannot create data directory: " + dataDir);

        }

        if (!aviDir.mkdirs() && !aviDir.isDirectory()) {

            Log.e(TAG, "Cannot create cinematic directory: " + aviDir);

        }



        AssetManager assets = getAssets();

        copyAsset(assets, "textures/textures.bar", new File(texturesDir, "textures.bar"), false);

        copyAsset(assets, "data/data.bar", new File(dataDir, "data.bar"), false);

        for (String videoName : ALL_KNOWN_VIDEOS) {

            copyAsset(assets, "avi/" + videoName, new File(aviDir, videoName), true);

        }

    }



    private boolean copyAsset(AssetManager assets, String assetPath, File output, boolean optional) {

        if (output.isFile()) {

            return true;

        }



        File temporary = new File(output.getPath() + ".tmp");

        try (InputStream input = assets.open(assetPath);

             FileOutputStream stream = new FileOutputStream(temporary)) {

            byte[] buffer = new byte[8192];

            int count;

            while ((count = input.read(buffer)) != -1) {

                stream.write(buffer, 0, count);

            }

            stream.getFD().sync();

            if (!temporary.renameTo(output)) {

                throw new IOException("Unable to move temporary asset into place: " + output);

            }

            return true;

        } catch (FileNotFoundException e) {

            if (optional) {

                Log.i(TAG, "Converted intro video is not packaged: " + assetPath);

                return false;

            }

            Log.e(TAG, "Required game asset is missing: " + assetPath, e);

        } catch (IOException e) {

            temporary.delete();

            Log.e(TAG, "Failed to copy asset " + assetPath, e);

        }

        return false;

    }



    private void playIntroCinematics() {

        File externalDir = getExternalFilesDir(null);

        if (externalDir == null) {

            return;

        }



        File aviDir = new File(new File(externalDir, "AoM"), "avi");

        for (String videoName : INTRO_VIDEO_NAMES) {

            File video = new File(aviDir, videoName);

            if (video.isFile() && video.length() > 0) {

                introVideos.add(video);

            }

        }

        if (introVideos.isEmpty()) {

            Log.i(TAG, "No converted intro videos found; continuing with the native splash screen");

            return;

        }



        startCinematicDialog();

    }



    private void startCinematicDialog() {

        if (cinematicOverlay != null || introVideos.isEmpty()) {

            return;

        }



        cinematicOverlay = new FrameLayout(this);

        cinematicOverlay.setBackgroundColor(0xFF000000);

        cinematicVideo = new VideoView(this);

        FrameLayout.LayoutParams videoLayout = new FrameLayout.LayoutParams(

                ViewGroup.LayoutParams.MATCH_PARENT,

                ViewGroup.LayoutParams.MATCH_PARENT,

                Gravity.CENTER);

        cinematicOverlay.addView(cinematicVideo, videoLayout);



        Button skipButton = new Button(this);

        skipButton.setText("Pular vídeo");

        FrameLayout.LayoutParams skipLayout = new FrameLayout.LayoutParams(

                ViewGroup.LayoutParams.WRAP_CONTENT,

                ViewGroup.LayoutParams.WRAP_CONTENT,

                Gravity.TOP | Gravity.END);

        int margin = (int) (16.0f * getResources().getDisplayMetrics().density);

        skipLayout.setMargins(margin, margin, margin, margin);

        cinematicOverlay.addView(skipButton, skipLayout);

        skipButton.setOnClickListener(view -> finishIntroCinematics());



        cinematicDialog = new Dialog(this, android.R.style.Theme_NoTitleBar_Fullscreen);

        cinematicDialog.setCancelable(false);

        cinematicDialog.setContentView(cinematicOverlay);

        cinematicDialog.show();

        Window window = cinematicDialog.getWindow();

        if (window != null) {

            window.setBackgroundDrawableResource(android.R.color.black);

            window.setLayout(

                    ViewGroup.LayoutParams.MATCH_PARENT,

                    ViewGroup.LayoutParams.MATCH_PARENT);

        }



        cinematicVideo.setOnCompletionListener(this::playNextIntroVideo);

        cinematicVideo.setOnErrorListener((player, what, extra) -> {

            Log.e(TAG, "Cinematic video playback failed: what=" + what + ", extra=" + extra);

            playNextIntroVideo(player);

            return true;

        });

        playNextIntroVideo(null);

    }



    private void playNextIntroVideo(MediaPlayer player) {

        if (cinematicOverlay == null || cinematicVideo == null) {

            return;

        }

        if (introVideoIndex >= introVideos.size()) {

            finishIntroCinematics();

            return;

        }



        File video = introVideos.get(introVideoIndex++);

        Log.i(TAG, "Playing intro video: " + video.getName());

        cinematicVideo.setVideoPath(video.getAbsolutePath());

        cinematicVideo.start();

    }



    private void finishIntroCinematics() {

        if (cinematicVideo != null) {

            cinematicVideo.setOnCompletionListener(null);

            cinematicVideo.setOnErrorListener(null);

            cinematicVideo.stopPlayback();

            cinematicVideo = null;

        }

        if (cinematicOverlay != null) {

            if (cinematicDialog != null && cinematicDialog.isShowing()) {

                cinematicDialog.dismiss();

            }

            cinematicOverlay = null;

        }

        cinematicDialog = null;

        introVideos.clear();

    }



    @Override

    protected void onPause() {

        if (cinematicVideo != null && cinematicVideo.isPlaying()) {

            cinematicVideo.pause();

        }

        super.onPause();

    }



    @Override

    protected void onResume() {

        super.onResume();

        if (cinematicVideo != null && !cinematicVideo.isPlaying()) {

            cinematicVideo.start();

        }

    }



    @Override

    protected void onDestroy() {

        finishIntroCinematics();

        super.onDestroy();

    }



    private static final int REQUEST_GAME_FOLDER = 2001;
    private static final String PREFS = "aom_native_settings";

    private File gameDataRoot() {
        File externalDir = getExternalFilesDir(null);
        return externalDir == null ? null : new File(externalDir, "AoM");
    }

    private boolean hasImportedGameData() {
        File root = gameDataRoot();
        return root != null && (new File(root, "data/data.bar").isFile()
                || new File(root, "textures/textures.bar").isFile()
                || new File(root, "models/models.bar").isFile());
    }

    private void chooseGameFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(intent, REQUEST_GAME_FOLDER);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_GAME_FOLDER || resultCode != RESULT_OK || data == null) return;
        Uri tree = data.getData();
        if (tree == null) return;
        try {
            getContentResolver().takePersistableUriPermission(tree, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString("game_tree", tree.toString()).apply();
            if (importGameTree(tree)) {
                android.widget.Toast.makeText(this, "Dados importados. Reiniciando...", android.widget.Toast.LENGTH_LONG).show();
                new android.os.Handler().postDelayed(this::recreate, 700);
            } else {
                android.widget.Toast.makeText(this, "Pasta inválida: BARs principais não encontrados.", android.widget.Toast.LENGTH_LONG).show();
            }
        } catch (SecurityException e) {
            Log.e(TAG, "Unable to access selected game folder", e);
        }
    }

    private boolean importGameTree(Uri tree) {
        File root = gameDataRoot();
        if (root == null) return false;
        if (!root.exists() && !root.mkdirs()) return false;
        return copyTree(tree, root) && hasImportedGameData();
    }

    private boolean copyTree(Uri tree, File destination) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree));
        String[] projection = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE};
        try (Cursor cursor = getContentResolver().query(children, projection, null, null, null)) {
            if (cursor == null) return false;
            while (cursor.moveToNext()) {
                Uri child = DocumentsContract.buildDocumentUriUsingTree(tree, cursor.getString(0));
                File output = new File(destination, cursor.getString(1));
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2))) {
                    if (!output.exists() && !output.mkdirs()) return false;
                    if (!copyTree(child, output)) return false;
                } else if (!copyDocument(child, output)) return false;
            }
            return true;
        }
    }

    private boolean copyDocument(Uri source, File output) {
        File temporary = new File(output.getPath() + ".tmp");
        try (InputStream input = getContentResolver().openInputStream(source);
             FileOutputStream stream = new FileOutputStream(temporary)) {
            if (input == null) return false;
            byte[] buffer = new byte[1024 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) stream.write(buffer, 0, count);
            stream.getFD().sync();
            if (output.exists() && !output.delete()) return false;
            return temporary.renameTo(output);
        } catch (IOException | SecurityException e) {
            temporary.delete();
            Log.e(TAG, "Failed importing " + source, e);
            return false;
        }
    }
}
