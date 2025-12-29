package de.citec.clfvr.bart;

import org.opencv.core.Core;
import org.opencv.core.Mat;
import org.opencv.highgui.*;

import java.awt.FlowLayout;
import java.awt.Image;
import java.awt.image.BufferedImage;
import java.awt.image.DataBufferByte;

import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;

/**
 * Hello world!
 *
 */
public class TestMarkerTracker {

	// Creates a new MarkerTracker and the corresponding C++ instance
	public TestMarkerTracker() {
	}

	static {
		System.loadLibrary(Core.NATIVE_LIBRARY_NAME);
		System.loadLibrary("BART_JNI");
	}

	// Test the basic MarkerTracker
	public static void main(String[] args) {
		MarkerTracker mt = new MarkerTracker();
		mt.configure();

		VideoCapture camera = new VideoCapture(0);
		camera.open(0); // Useless
		if (!camera.isOpened()) {
			System.out.println("Camera Error");
		} else {
			System.out.println("Camera OK?");
		}

		Mat frame = new Mat();
		boolean updated;
		createImageFrame();
		while (jframe.isActive()) {
			camera.read(frame);
			System.out.println("Frame Obtained");
			System.out.println("cols:" + frame.cols());
			System.out.println("rows:" + frame.rows());
			mt.processFrame(frame, 0.5, 2);
			Mat m = new Mat();
			mt.getBestMat(m);
			if(m.cols()>1){
				System.out.println("tiefe: " + m.depth());
				System.out.println("type: " + m.type());
				System.out.println("mat: " + m.cols() + "  " + m.rows() + "   " + m.dump());
				float[] data = new float[m.cols() * m.rows() * (int) m.elemSize()];
				System.out.println(data.length);
				System.out.println(m.get(0, 0, data));
				System.out.println(data[1]);
			}


			// System.out.println(m.cols() + " " +m.rows());
			// updated = false;
			// Mat tmpMat = m;
			//
			// while (!updated) {
			// if (m != tmpMat) {
			// updated = true;
			// } else {
			// mt.getBestMat(m);
			// }
			// }
			BufferedImage i = Mat2BufferedImage(frame);

			// BufferedImage i = Mat2BufferedImage(frame);
			updateImage(i);

			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		camera.release();
	}

	// ***************************************************************************
	// Start of native functions
	public static BufferedImage Mat2BufferedImage(Mat m) {
		// source:
		// http://answers.opencv.org/question/10344/opencv-java-load-image-to-gui/
		// Fastest code
		// The output can be assigned either to a BufferedImage or to an Image

		int type = BufferedImage.TYPE_BYTE_GRAY;
		if (m.channels() > 1) {
			type = BufferedImage.TYPE_3BYTE_BGR;
		}
		int bufferSize = m.channels() * m.cols() * m.rows();
		byte[] b = new byte[bufferSize];
		m.get(0, 0, b); // get all the pixels
		BufferedImage image = new BufferedImage(m.cols(), m.rows(), type);
		final byte[] targetPixels = ((DataBufferByte) image.getRaster().getDataBuffer()).getData();
		System.arraycopy(b, 0, targetPixels, 0, b.length);
		return image;

	}

	public static JFrame jframe;
	public static JLabel lbl;

	public static void createImageFrame() {
		jframe = new JFrame();
		jframe.setLayout(new FlowLayout());
		lbl = new JLabel();
		jframe.add(lbl);
		jframe.setVisible(true);
		jframe.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

	}

	public static void updateImage(Image img2) {
		ImageIcon icon = new ImageIcon(img2);
		jframe.setSize(img2.getWidth(null) + 50, img2.getHeight(null) + 50);
		lbl.setIcon(icon);
	}
}
