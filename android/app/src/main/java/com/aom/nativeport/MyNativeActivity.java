package com.aom.nativeport;



import android.Manifest;

import android.app.AlertDialog;

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

    /** Evita iniciar duas cópias simultâneas da pasta do jogo. */
    private volatile boolean importInProgress = false;



    @Override

    protected void onCreate(Bundle savedInstanceState) {

        copyGameAssetsIfNeeded();

        super.onCreate(savedInstanceState);

        playIntroCinematics();

        requestStorageAccessIfNeeded();

    }

    /**
     * Pede acesso ao armazenamento compartilhado apenas uma vez e somente se
     * ainda não houver dados importados na pasta privada do app. O importador
     * por SAF funciona sem essa permissão.
     */
    private void requestStorageAccessIfNeeded() {
        if (hasStoragePermission()) return;

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
        }
        requestPermissions(
                new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE},
                REQUEST_STORAGE);
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
        File modelsDir = new File(aomDir, "models");
        File scenarioDir = new File(aomDir, "scenario");
        File soundDir = new File(aomDir, "sound");
        File animDir = new File(aomDir, "anim");
        File godpowersDir = new File(aomDir, "godpowers");

        File[] dirs = {texturesDir, dataDir, aviDir, modelsDir, scenarioDir, soundDir, animDir, godpowersDir};
        for (File d : dirs) {
            if (!d.mkdirs() && !d.isDirectory()) {
                Log.e(TAG, "Cannot create directory: " + d);
            }
        }

        AssetManager assets = getAssets();
        // Texturas (Base + The Titans)
        copyAsset(assets, "textures/textures.bar", new File(texturesDir, "textures.bar"), true);
        copyAsset(assets, "textures/textures2.bar", new File(texturesDir, "textures2.bar"), true);

        // Dados do jogo (Base + The Titans)
        copyAsset(assets, "data/data.bar", new File(dataDir, "data.bar"), true);
        copyAsset(assets, "data/data2.bar", new File(dataDir, "data2.bar"), true);

        // Modelos 3D (Base + The Titans)
        copyAsset(assets, "models/models.bar", new File(modelsDir, "models.bar"), true);
        copyAsset(assets, "models/models2.bar", new File(modelsDir, "models2.bar"), true);

        // Cenários e campanhas
        copyAsset(assets, "scenario/scenarios.bar", new File(scenarioDir, "scenarios.bar"), true);
        copyAsset(assets, "scenario/scenarios2.bar", new File(scenarioDir, "scenarios2.bar"), true);

        // Sons e efeitos de áudio
        copyAsset(assets, "sound/sounds.bar", new File(soundDir, "sounds.bar"), true);
        copyAsset(assets, "sound/sounds2.bar", new File(soundDir, "sounds2.bar"), true);

        // Animações
        copyAsset(assets, "anim/anim.bar", new File(animDir, "anim.bar"), true);
        copyAsset(assets, "anim/anim2.bar", new File(animDir, "anim2.bar"), true);

        // Poderes divinos
        copyAsset(assets, "godpowers/godpowers.bar", new File(godpowersDir, "godpowers.bar"), true);
        copyAsset(assets, "godpowers/godpowers2.bar", new File(godpowersDir, "godpowers2.bar"), true);

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

                Log.i(TAG, "Optional packaged asset is not present: " + assetPath);

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

    /**
     * Chamado pela engine nativa (JNI) pelo botão "IMPORTAR DADOS DO JOGO".
     *
     * A engine roda em uma thread própria; por isso o trabalho de interface é
     * sempre publicado na thread principal com runOnUiThread().
     */
    public void requestGameDataImport() {
        runOnUiThread(() -> {
            if (importInProgress) {
                android.widget.Toast.makeText(this,
                        "A importação de dados já está em andamento.",
                        android.widget.Toast.LENGTH_LONG).show();
                return;
            }

            new AlertDialog.Builder(this)
                    .setTitle("Importar dados do jogo")
                    .setMessage("Selecione a pasta principal da sua cópia legal do "
                            + "Age of Mythology — a pasta que contém as subpastas "
                            + "\"data\" e \"textures\" (por exemplo: Age of Mythology) "
                            + "ou, em instalações antigas, os arquivos data.bar e "
                            + "textures.bar diretamente.\n\n"
                            + "Os arquivos serão copiados para o armazenamento privado "
                            + "do aplicativo. A operação pode levar alguns minutos; não "
                            + "feche o app. Não é necessário conceder acesso a todos os "
                            + "arquivos do dispositivo.")
                    .setPositiveButton("Escolher pasta",
                            (dialog, which) -> chooseGameFolder())
                    .setNegativeButton("Cancelar", null)
                    .show();
        });
    }

    /** Aceita a estrutura com subpastas e a estrutura plana (data.bar/textures.bar). */
    private boolean directoryContainsGameData(File dir) {
        return new File(dir, "data/data.bar").isFile()
                || new File(dir, "textures/textures.bar").isFile()
                || new File(dir, "data.bar").isFile()
                || new File(dir, "textures.bar").isFile()
                || new File(dir, "models/models.bar").isFile();
    }

    private boolean hasImportedGameData() {
        File root = gameDataRoot();
        if (root == null) return false;
        if (directoryContainsGameData(root)) return true;

        // Também aceita uma pasta de primeiro nível, por exemplo
        // .../files/AoM/Age of Mythology/{data,textures}.
        File[] children = root.listFiles();
        if (children == null) return false;
        for (File child : children) {
            if (child.isDirectory() && directoryContainsGameData(child)) return true;
        }
        return false;
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
            startGameDataImport(tree);
        } catch (SecurityException e) {
            Log.e(TAG, "Unable to access selected game folder", e);
            android.widget.Toast.makeText(this,
                    "Não foi possível acessar a pasta selecionada.",
                    android.widget.Toast.LENGTH_LONG).show();
        }
    }

    private void startGameDataImport(final Uri tree) {
        if (importInProgress) return;

        final File root = gameDataRoot();
        if (root == null) {
            android.widget.Toast.makeText(this,
                    "Armazenamento externo do aplicativo indisponível.",
                    android.widget.Toast.LENGTH_LONG).show();
            return;
        }

        importInProgress = true;
        android.widget.Toast.makeText(this,
                "Importando dados do jogo. Não feche o aplicativo...",
                android.widget.Toast.LENGTH_LONG).show();

        // A cópia de vários gigabytes não pode rodar na thread de UI (ANR).
        new Thread(() -> {
            boolean copied = false;
            try {
                if (!root.exists() && !root.mkdirs()) {
                    throw new IOException("Cannot create import directory: " + root);
                }
                copied = copyTree(tree, root);
            } catch (RuntimeException e) {
                Log.e(TAG, "Game data import failed", e);
            } catch (IOException e) {
                Log.e(TAG, "Game data import failed", e);
            }

            // A cópia é "best effort": um arquivo opcional inacessível não pode
            // invalidar a importação se os BARs principais chegaram ao destino.
            final boolean imported = hasImportedGameData();
            Log.i(TAG, "Game data import finished (tree copy complete=" + copied + ", usable=" + imported + ")");
            runOnUiThread(() -> {
                importInProgress = false;
                if (imported) {
                    android.widget.Toast.makeText(this,
                            "Dados importados. Reiniciando o jogo...",
                            android.widget.Toast.LENGTH_LONG).show();
                    new android.os.Handler(android.os.Looper.getMainLooper())
                            .postDelayed(this::recreate, 800);
                } else {
                    android.widget.Toast.makeText(this,
                            "Pasta inválida. Selecione a pasta que contém "
                                    + "data/data.bar e textures/textures.bar.",
                            android.widget.Toast.LENGTH_LONG).show();
                }
            });
        }, "aom-data-import").start();
    }

    private boolean copyTree(Uri tree, File destination) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree));
        String[] projection = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE};
        boolean complete = true;
        try (Cursor cursor = getContentResolver().query(children, projection, null, null, null)) {
            if (cursor == null) return false;
            while (cursor.moveToNext()) {
                Uri child = DocumentsContract.buildDocumentUriUsingTree(tree, cursor.getString(0));
                File output = new File(destination, cursor.getString(1));
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2))) {
                    if (!output.exists() && !output.mkdirs()) {
                        Log.e(TAG, "Cannot create import subdirectory: " + output);
                        complete = false;
                        continue;
                    }
                    if (!copyTree(child, output)) complete = false;
                } else if (!copyDocument(child, output)) {
                    complete = false;
                }
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to enumerate import tree", e);
            return false;
        }
        return complete;
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
