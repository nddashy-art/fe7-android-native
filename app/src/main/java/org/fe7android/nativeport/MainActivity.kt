package org.fe7android.nativeport

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Temporary launcher screen. This is NOT the game and does not yet load FE7 code or assets.
 * Keeping the first APK deliberately small lets CI verify the Android toolchain before porting.
 */
class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(28, 24, 28, 24)
            setBackgroundColor(0xFF151923.toInt())
        }

        val title = TextView(this).apply {
            text = "FE7 ANDROID NATIVE"
            textSize = 26f
            gravity = Gravity.CENTER
            setTextColor(0xFFFFD98A.toInt())
        }

        val status = TextView(this).apply {
            text = "Android shell is running.\n\nThe game engine is not integrated yet.\nNo ROM or copyrighted game data is included in this app."
            textSize = 16f
            gravity = Gravity.CENTER
            setTextColor(0xFFF1F3F8.toInt())
            setLineSpacing(8f, 1.0f)
        }

        val hint = TextView(this).apply {
            text = "Next: native build integration, then boot the title screen."
            textSize = 13f
            gravity = Gravity.CENTER
            setTextColor(0xFFB7C0D4.toInt())
        }

        root.addView(title, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = 28 })
        root.addView(status, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = 28 })
        root.addView(hint, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ))

        setContentView(root)
    }
}
