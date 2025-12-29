package de.citec.clfvr.bart;

import org.opencv.core.Mat;
import org.opencv.core.*;

/**
 * Hello world!
 *
 */
public class MarkerTracker {
	// Holds the pointer to the original C++ object...
	private long _ptr;

	// Creates a new MarkerTracker and the corresponding C++ instance
	public MarkerTracker() {
		_ptr = create();

		// Mat m = new Mat();
	}

	// Destroys the MarkerTracker and releases its C++ instance
	public void destroy() {
		destroy(_ptr);
	}

	// Configures the MarkerTracker
	public void configure() {
		configure(_ptr);
	}

	// Processes one frame
	public void processFrame(Mat mat, double downScaleFactor, int scales) {
		processFrame(_ptr, mat.getNativeObjAddr(), downScaleFactor, scales);
	}

	static {
		System.loadLibrary(Core.NATIVE_LIBRARY_NAME);
		System.loadLibrary("BART_JNI");
	}

	public void getBestMat(Mat mat) {
		getBestMat(_ptr, mat.getNativeObjAddr());
	}

	// ***************************************************************************
	// Start of native functions

	// Creates a new MarkerTracker object
	private native long create();

	// Destroys a MarkerTracker object
	private native void destroy(long ptr);

	// Configures the MarkerTracker
	private native void configure(long ptr);

	// Process a frame
	private native void processFrame(long ptr, long image, double downScaleFactor, int scales);

	// get current frame
	private native void getBestMat(long ptr, long mat);
}
