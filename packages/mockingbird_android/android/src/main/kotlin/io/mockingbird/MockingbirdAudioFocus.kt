// Android audio focus for the player, which Oboe has no part in: requested
// when a player starts and abandoned when it stops, with every focus change
// and the becoming-noisy broadcast passed to Dart by name.
package io.mockingbird

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build
import io.flutter.plugin.common.EventChannel

/** Holds playback focus for one Flutter engine and reports what happens to it. */
class MockingbirdAudioFocus(private val context: Context) : AudioManager.OnAudioFocusChangeListener {
    private val audio = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var sink: EventChannel.EventSink? = null
    private var request: AudioFocusRequest? = null
    private var held = false
    private val noisy = object : BroadcastReceiver() {
        override fun onReceive(c: Context?, intent: Intent?) {
            if (intent?.action == AudioManager.ACTION_AUDIO_BECOMING_NOISY) send("becomingNoisy")
        }
    }

    /** Where focus changes go; null drops them. */
    fun listen(events: EventChannel.EventSink?) {
        sink = events
    }

    /** Requests focus for spoken media; a delayed grant reports a transient loss until it arrives. */
    fun request(): Boolean {
        if (held) return true
        held = true
        val filter = IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
        if (Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(noisy, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            context.registerReceiver(noisy, filter)
        }
        val result = if (Build.VERSION.SDK_INT >= 26) requestModern() else requestLegacy()
        if (result == AudioManager.AUDIOFOCUS_REQUEST_DELAYED) send("lostTransient")
        return result != AudioManager.AUDIOFOCUS_REQUEST_FAILED
    }

    /** Gives focus back and stops listening for the noisy broadcast. */
    fun abandon() {
        if (!held) return
        held = false
        context.unregisterReceiver(noisy)
        if (Build.VERSION.SDK_INT >= 26) {
            request?.let { audio.abandonAudioFocusRequest(it) }
        } else {
            @Suppress("DEPRECATION")
            audio.abandonAudioFocus(this)
        }
    }

    override fun onAudioFocusChange(change: Int) {
        when (change) {
            AudioManager.AUDIOFOCUS_GAIN -> send("gained")
            AudioManager.AUDIOFOCUS_LOSS -> send("lost")
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> send("lostTransient")
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> send("duck")
        }
    }

    private fun requestModern(): Int {
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build()
        val r = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(attributes)
            .setAcceptsDelayedFocusGain(true)
            .setOnAudioFocusChangeListener(this)
            .build()
        request = r
        return audio.requestAudioFocus(r)
    }

    @Suppress("DEPRECATION")
    private fun requestLegacy(): Int =
        audio.requestAudioFocus(this, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)

    private fun send(name: String) {
        sink?.success(name)
    }
}
