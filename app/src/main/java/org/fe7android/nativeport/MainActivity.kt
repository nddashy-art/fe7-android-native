package org.fe7android.nativeport

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Prototype launcher with a user-selected ROM document.
 *
 * The ROM stays wherever the user selected it from. This app does not upload or copy it.
 * The game engine is not integrated yet, so selecting a ROM only remembers its URI for a
 * future engine integration; it does not make the game playable by itself.
 */
class MainActivity : Activity() {
    private lateinit var romStatus: TextView

    companion object {
        private const val REQUEST_OPEN_ROM = 4107
        private const val PREFS = "fe7_settings"
        private const val KEY_ROM_URI = "selected_rom_uri"
        private const val KEY_ROM_NAME = "selected_rom_name"
    }

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

        val description = TextView(this).apply {
            text = "Selecciona tu propia ROM legal de Fire Emblem: The Blazing Blade (US).\n\nLa app no incluye una ROM y no la sube a internet."
            textSize = 16f
            gravity = Gravity.CENTER
            setTextColor(0xFFF1F3F8.toInt())
            setLineSpacing(6f, 1.0f)
        }

        val chooseButton = Button(this).apply {
            text = "Seleccionar ROM (.gba)"
            setOnClickListener { openRomPicker() }
        }

        romStatus = TextView(this).apply {
            textSize = 14f
            gravity = Gravity.CENTER
            setTextColor(0xFFB7C0D4.toInt())
            setLineSpacing(5f, 1.0f)
        }

        val warning = TextView(this).apply {
            text = "Aviso: esta versión solo recuerda el archivo elegido. El motor del juego aún no está integrado, así que la ROM todavía no se puede ejecutar."
            textSize = 12f
            gravity = Gravity.CENTER
            setTextColor(0xFFFFD98A.toInt())
            setLineSpacing(4f, 1.0f)
        }

        root.addView(title, layoutParams(bottom = 22))
        root.addView(description, layoutParams(bottom = 18))
        root.addView(chooseButton, layoutParams(bottom = 16))
        root.addView(romStatus, layoutParams(bottom = 18))
        root.addView(warning, layoutParams())

        setContentView(root)
        restoreSavedRom()
    }

    private fun layoutParams(bottom: Int = 0) =
        LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = bottom }

    private fun openRomPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_OPEN_ROM)
    }

    @Deprecated("Uses startActivityForResult for compatibility with the current minimal project.")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_OPEN_ROM || resultCode != RESULT_OK) return

        val uri: Uri = data?.data ?: return
        val displayName = queryDisplayName(uri) ?: uri.lastPathSegment ?: "archivo seleccionado"

        if (!displayName.lowercase().endsWith(".gba")) {
            romStatus.text = "Archivo seleccionado: $displayName\nParece que no termina en .gba. Selecciona una ROM GBA válida."
            return
        }

        try {
            val flags = data?.flags ?: 0
            val readFlags = flags and (Intent.FLAG_GRANT_READ_URI_PERMISSION)
            if (readFlags != 0) {
                contentResolver.takePersistableUriPermission(uri, readFlags)
            }
        } catch (_: SecurityException) {
            // Some document providers do not support persistable grants; keep this session's URI.
        } catch (_: IllegalArgumentException) {
            // Provider did not offer a persistable permission.
        }

        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
            .putString(KEY_ROM_URI, uri.toString())
            .putString(KEY_ROM_NAME, displayName)
            .apply()

        romStatus.text = "ROM seleccionada: $displayName\nLa ubicación se guardó en este dispositivo. El motor todavía no la está leyendo."
    }

    private fun restoreSavedRom() {
        val prefs = getSharedPreferences(PREFS, MODE_PRIVATE)
        val uri = prefs.getString(KEY_ROM_URI, null)
        val name = prefs.getString(KEY_ROM_NAME, null)
        romStatus.text = if (uri != null && name != null) {
            // A previously granted document URI may become unavailable if the user moves/deletes it.
            val readable = try {
                contentResolver.openFileDescriptor(Uri.parse(uri), "r")?.use { true } ?: false
            } catch (_: Exception) {
                false
            }
            if (readable) {
                "ROM guardada: $name\nEl motor todavía no la está leyendo."
            } else {
                "La ROM guardada ya no está disponible. Selecciónala otra vez."
            }
        } else {
            "Todavía no has seleccionado una ROM."
        }
    }

    private fun queryDisplayName(uri: Uri): String? {
        contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0) return cursor.getString(index)
            }
        }
        return null
    }
}
