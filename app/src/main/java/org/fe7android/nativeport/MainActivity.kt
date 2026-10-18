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
import java.io.IOException
import java.util.Locale

/**
 * Prototype launcher with a user-selected ROM document.
 * The ROM remains at its chosen location; this app does not upload or copy it.
 * This screen reads only the GBA header. The game engine is not integrated yet.
 */
class MainActivity : Activity() {
    private lateinit var romStatus: TextView

    companion object {
        private const val REQUEST_OPEN_ROM = 4107
        private const val PREFS = "fe7_settings"
        private const val KEY_ROM_URI = "selected_rom_uri"
        private const val KEY_ROM_NAME = "selected_rom_name"
        private const val GBA_HEADER_SIZE = 0xB0
        private const val MIN_GBA_SIZE = 0xC0
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
            text = "Selecciona tu propia ROM de Fire Emblem: The Blazing Blade (US).\n\nLa app no incluye una ROM ni la sube a internet."
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
            text = "Prototipo: se comprueba la cabecera del archivo, pero el motor del juego aún no está integrado y la ROM todavía no se puede ejecutar."
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

        if (!displayName.lowercase(Locale.ROOT).endsWith(".gba")) {
            romStatus.text = "Archivo seleccionado: $displayName\nSelecciona un archivo .gba."
            return
        }

        val header = try {
            readGbaHeader(uri)
        } catch (_: Exception) {
            romStatus.text = "No se pudo leer el archivo. Comprueba los permisos y vuelve a intentarlo."
            return
        }

        if (header == null) {
            romStatus.text = "El archivo es demasiado pequeño o no se pudo leer como una ROM GBA."
            return
        }

        try {
            val grantedFlags = ((data?.flags) ?: 0) and Intent.FLAG_GRANT_READ_URI_PERMISSION
            if (grantedFlags != 0) contentResolver.takePersistableUriPermission(uri, grantedFlags)
        } catch (_: SecurityException) {
            // Some providers do not support persisted grants.
        } catch (_: IllegalArgumentException) {
            // The provider did not offer a persistable permission.
        }

        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
            .putString(KEY_ROM_URI, uri.toString())
            .putString(KEY_ROM_NAME, displayName)
            .apply()

        val sizeText = if (header.sizeBytes > 0) formatSize(header.sizeBytes) else "tamaño desconocido"
        val identity = if (header.title.replace(" ", "").contains("FIREEMBLEM", ignoreCase = true)) {
            "La cabecera parece corresponder a Fire Emblem."
        } else {
            "No se pudo confirmar que sea Fire Emblem; se guardó de todos modos."
        }
        romStatus.text = "Archivo: ${displayName}\nTítulo: ${header.title.ifBlank { "(vacío)" }}\nCódigo: ${header.gameCode.ifBlank { "(vacío)" }} · $sizeText\n$identity\nGuardado localmente; aún no se ejecuta."
    }

    private data class GbaHeader(val title: String, val gameCode: String, val sizeBytes: Long)

    @Throws(IOException::class)
    private fun readGbaHeader(uri: Uri): GbaHeader? {
        val bytes = ByteArray(GBA_HEADER_SIZE)
        val input = contentResolver.openInputStream(uri) ?: return null
        val readCount = input.use { stream ->
            var total = 0
            while (total < bytes.size) {
                val count = stream.read(bytes, total, bytes.size - total)
                if (count <= 0) break
                total += count
            }
            total
        }
        if (readCount < MIN_GBA_SIZE) return null

        val title = bytes.copyOfRange(0xA0, 0xAC).decodeToString().trim('\u0000', ' ')
        val gameCode = bytes.copyOfRange(0xAC, 0xB0).decodeToString().trim('\u0000', ' ')
        return GbaHeader(title, gameCode, queryFileSize(uri))
    }

    private fun queryFileSize(uri: Uri): Long {
        return try {
            contentResolver.query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val index = cursor.getColumnIndex(OpenableColumns.SIZE)
                    if (index >= 0 && !cursor.isNull(index)) cursor.getLong(index) else -1L
                } else -1L
            } ?: -1L
        } catch (_: Exception) {
            -1L
        }
    }

    private fun formatSize(bytes: Long): String =
        String.format(Locale.ROOT, "%.2f MiB", bytes / (1024.0 * 1024.0))

    private fun restoreSavedRom() {
        val prefs = getSharedPreferences(PREFS, MODE_PRIVATE)
        val uriText = prefs.getString(KEY_ROM_URI, null)
        val name = prefs.getString(KEY_ROM_NAME, null)
        romStatus.text = if (uriText != null && name != null) {
            try {
                val header = readGbaHeader(Uri.parse(uriText))
                if (header != null) {
                    "ROM guardada: $name\nTítulo: ${header.title.ifBlank { "(vacío)" }} · Código: ${header.gameCode.ifBlank { "(vacío)" }}\nEl motor aún no la ejecuta."
                } else {
                    "El archivo guardado ya no parece legible. Selecciónalo otra vez."
                }
            } catch (_: Exception) {
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
