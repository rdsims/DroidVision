# custom rules to allow JNI degugging from within Android Studio

# the following is copied from:
# https://stackoverflow.com/questions/28836243/android-studios-debugger-not-stopping-at-breakpoints-within-library-modules

# The support library contains references to newer platform versions.
# Don't warn about those in case this app is linking against an older
# platform version.  We know about them, and they are safe.
-dontwarn android.support.**

-keep class !android.support.v7.internal.view.menu.**,android.support.** {*;}
-ignorewarnings
-renamesourcefileattribute SourceFile
-keepattributes SourceFile,LineNumberTable

-keep public class * extends android.support.v4.** {*;}
-keep public class * extends android.app.Fragment