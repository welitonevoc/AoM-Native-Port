# Proguard / R8 rules for AoM Native Port

# Keep NativeActivity subclass and entry points
-keep public class com.aom.nativeport.MyNativeActivity { *; }
-keep public class * extends android.app.NativeActivity { *; }

# Keep native methods and callbacks
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep members accessed by reflection / JNI
-keepclassmembers class * {
    public void *(android.view.View);
}

-dontwarn android.app.**
-dontwarn android.media.**
-dontwarn android.view.**
-dontwarn java.lang.invoke.**

