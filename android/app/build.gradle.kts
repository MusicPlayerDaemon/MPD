plugins {
    id("com.android.application")
}

android {
    namespace = "org.musicpd"
    compileSdk = 34

    defaultConfig {
        applicationId = "org.musicpd"
        minSdk = 24
        targetSdk = 30
        versionCode = 1
        versionName = "1.0"
    }

    buildFeatures {
        aidl = true
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
}