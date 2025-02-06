plugins {
    id("com.google.devtools.ksp")
    alias(libs.plugins.android.application)
    alias(libs.plugins.jetbrains.kotlin.android)
    alias(libs.plugins.dagger.hilt.android)
}

android {
    namespace = "org.musicpd"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.musicpd"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        vectorDrawables {
            useSupportLibrary = true
        }
    }

    buildFeatures {
        aidl = true
        compose = true
    }

    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.10"
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    // flavors
    flavorDimensions += "base"
    productFlavors {
        create("fail-test") {
            // To test System.loadLibrary("mpd") failure
            // exclude the native lib from the package
            packaging {
                jniLibs {
                    // it appears the 'excludes' is applied to all flavors
                    // even if it's only inside this flavor.
                    // this filters by task name to apply the exclusion only
                    // for this flavor name.
                    // (clearing the 'abiFilters' will only create a universal apk
                    // with all of the abi versions)
                    gradle.startParameter.getTaskNames().forEach { task ->
                        if (task.contains("fail-test", ignoreCase = true)) {
                            println("NOTICE: excluding libmpd.so from package $task for testing")
                            excludes += "**/libmpd.so"
                        }
                    }
                }
            }
        }
        create("arm64-v8a") {
            ndk {
                // ABI to include in package
                //noinspection ChromeOsAbiSupport
                abiFilters += listOf("arm64-v8a")
            }
        }
        create("x86_64") {
            ndk {
                // ABI to include in package
                abiFilters += listOf("x86_64")
            }
        }
        create("universal") {
            ndk {
                // ABI to include in package
                abiFilters += listOf("arm64-v8a", "x86_64")
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_9
        targetCompatibility = JavaVersion.VERSION_1_9
    }
    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_1_9.toString()
    }
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

dependencies {
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(platform(libs.androidx.compose.bom))

    implementation(libs.androidx.material3)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.material.icons.extended)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.navigation.compose)

    implementation(libs.compose.settings.ui.m3)
    implementation(libs.compose.settings.storage.preferences)
    implementation(libs.accompanist.permissions)

    implementation(libs.hilt.android)
    ksp(libs.dagger.compiler)
    ksp(libs.hilt.compiler)

    implementation(libs.androidx.media3.session)

    // Android Studio Preview support
    implementation(libs.androidx.ui.tooling.preview)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)

    implementation(libs.androidx.appcompat)
}
