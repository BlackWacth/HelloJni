package com.hzw.hellojni

import android.os.Build
import android.util.Log
import androidx.annotation.Keep
import java.util.*

class JniHandler {

    @Keep
    private fun updateStatus(msg: String) {
        if (msg.lowercase(Locale.getDefault()).contains("error")) {
            Log.e("JniHandler", "Native Err: $msg")
        } else {
            Log.i("JniHandler", "Native Msg: $msg")
        }
    }

    @Keep
    fun getRuntimeMemorySize(): Long {
        return Runtime.getRuntime().freeMemory()
    }

    companion object {
        @Keep
        @JvmStatic
        fun getBuildVersion(): String {
            return Build.VERSION.RELEASE;
        }
    }
}