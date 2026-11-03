import pathlib
manifest = """<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.aom.nativeport" android:versionCode="1" android:versionName="1.0">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="33" />
  <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
  <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
  <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
  <application android:label="AoM Native" android:icon="@mipmap/ic_launcher" android:hasCode="false" android:extractNativeLibs="true" android:requestLegacyExternalStorage="true" android:theme="@android:style/Theme.NoTitleBar.Fullscreen">
    <activity android:name="android.app.NativeActivity" android:exported="true" android:screenOrientation="landscape" android:configChanges="orientation|keyboardHidden|screenSize">
      <meta-data android:name="android.app.lib_name" android:value="aom_native" />
      <intent-filter><action android:name="android.intent.action.MAIN" /><category android:name="android.intent.category.LAUNCHER" /></intent-filter>
    </activity>
  </application>
</manifest>
"""
pathlib.Path(r"E:\Port\AoM-Native-Port\build-apk\AndroidManifest.xml").write_text(manifest, encoding="utf-8")
print("written")
