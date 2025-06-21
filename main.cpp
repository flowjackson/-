#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <locale>
#include <string>
#include <fstream>  // 添加文件流支持
#include <windows.h>


#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QSlider>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QStack>
#include <QToolBar>
#include <QToolButton>
#include <QStyle>
#include <QIcon>
#include <opencv2/opencv.hpp>


using namespace std;
using namespace cv;

class ImageProcessor : public QMainWindow {
    Q_OBJECT

public:
    ImageProcessor(QWidget* parent = nullptr) : QMainWindow(parent) {
        // 初始化UI
        setupUI();

        // 初始化变量
        currentImage = Mat();
        originalImage = Mat();
        isImageLoaded = false;
        zoomFactor = 1.0;

        // 初始化撤销栈
        undoStack.setMaxCount(10);

        // 连接信号槽
        connectActions();

        // 设置状态栏
        setupStatusBar();
    }

private slots:
    // 全局HOG检测器初始化
    HOGDescriptor initHOGDetector() {
        static bool initialized = false;
        static HOGDescriptor hog;

        if (!initialized) {
            hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());
            initialized = true;
        }
        return hog;
    }

    /**
     * @brief 行人检测函数
     * @param imagePath 要处理的图片路径
     * @param verbose 是否显示处理信息（可选，默认false）
     * @return 处理后的图像（带有检测框）
     */
    Mat detectPeople(const string& imagePath, bool verbose = false) {
        // 初始化HOG检测器
        HOGDescriptor hog = initHOGDetector();

        // 读取图像
        Mat img = imread(imagePath);

        if (img.empty()) {
            if (verbose) {
                cerr << "错误: 无法加载图像: " << imagePath << endl;
            }
            return Mat(); // 返回空矩阵
        }

        // 行人检测
        vector<Rect> found, found_filtered;
        double t = (double)getTickCount();
        hog.detectMultiScale(img, found, 0, Size(8, 8), Size(32, 32), 1.05, 2);
        t = (double)getTickCount() - t;

        if (verbose) {
            cout << "处理: " << imagePath << endl;
            cout << "检测时间: " << t * 1000. / getTickFrequency() << " 毫秒" << endl;
            cout << "发现候选区域: " << found.size() << endl;
        }

        // 过滤重叠检测
        for (size_t i = 0; i < found.size(); i++) {
            Rect r = found[i];
            bool inside = false;
            for (size_t j = 0; j < found.size(); j++) {
                if (j != i && (r & found[j]) == r) {
                    inside = true;
                    break;
                }
            }
            if (!inside) {
                found_filtered.push_back(r);
            }
        }

        if (verbose) {
            cout << "过滤后区域: " << found_filtered.size() << endl;
        }

        // 绘制结果
        for (const auto& r : found_filtered) {
            Rect adjusted(
                r.x + cvRound(r.width * 0.1),
                r.y + cvRound(r.height * 0.07),
                cvRound(r.width * 0.8),
                cvRound(r.height * 0.8)
            );
            rectangle(img, adjusted, Scalar(0, 255, 0), 3);
        }

        return img;
    }

    void detect() {
        if (!filepath.isEmpty()) {
                        // 调用行人检测函数
            Mat resultImage = detectPeople(filepath.toStdString(), true);
            if (!resultImage.empty()) {
                // 显示检测结果
                currentImage = resultImage.clone();
                displayImage(currentImage);
                statusBar()->showMessage("行人检测完成", 3000);
                updateStatusBar();
            } else {
                QMessageBox::warning(this, "错误", "行人检测失败或图像为空");
            }
        }
        else {
            QMessageBox::warning(this, "错误", "请先打开一张图像");
        }
	}

    // 文件操作
    void openImage() {
        QString fileName = QFileDialog::getOpenFileName(this, "打开图像", "",
            "图像文件 (*.png *.jpg *.bmp *.tif)");
		filepath = fileName; // 保存文件路径
        if (!fileName.isEmpty()) {
            Mat img = imread(fileName.toStdString(), IMREAD_COLOR);
            if (!img.empty()) {
                // 保存原始图像和当前图像
                originalImage = img.clone();
                currentImage = img.clone();
                isImageLoaded = true;

                // 清除撤销栈
                undoStack.clear();

                // 重置缩放
                zoomFactor = 1.0;
                zoomSlider->setValue(100);

                // 显示图像
                displayImage(currentImage);
                statusBar()->showMessage("已加载图像: " + fileName, 3000);

                // 更新状态栏信息
                updateStatusBar();
            }
            else {
                QMessageBox::warning(this, "错误", "无法打开图像文件");
            }
        }
    }

    void saveImage() {
        if (!isImageLoaded) {
            QMessageBox::warning(this, "错误", "没有图像可保存");
            return;
        }

        QString fileName = QFileDialog::getSaveFileName(this, "保存图像", "",
            "PNG图像 (*.png);;JPEG图像 (*.jpg);;BMP图像 (*.bmp)");
        if (!fileName.isEmpty()) {
            if (imwrite(fileName.toStdString(), currentImage)) {
                statusBar()->showMessage("图像已保存: " + fileName, 3000);
            }
            else {
                QMessageBox::warning(this, "错误", "保存图像失败");
            }
        }
    }

    void closeImage() {
        if (!isImageLoaded) return;

        currentImage = Mat();
        originalImage = Mat();
        isImageLoaded = false;
        imageLabel->clear();
        imageLabel->setText("无图像");
        undoStack.clear();

        // 重置缩放
        zoomFactor = 1.0;
        zoomSlider->setValue(100);

        statusBar()->showMessage("图像已关闭", 2000);

        // 更新状态栏信息
        updateStatusBar();
    }

    // 编辑操作
    void undo() {
        if (undoStack.isEmpty()) return;

        // 从撤销栈中恢复图像
        currentImage = undoStack.pop();
        displayImage(currentImage);
        statusBar()->showMessage("撤销操作", 1500);

        // 更新状态栏信息
        updateStatusBar();
    }

    void adjustParameters() {
        if (!isImageLoaded) return;

        // 创建对话框
        QDialog dialog(this);
        dialog.setWindowTitle("调整图像参数");

        // 创建控件
        QSlider brightnessSlider(Qt::Horizontal);
        brightnessSlider.setRange(-100, 100);
        brightnessSlider.setValue(0);

        QSlider contrastSlider(Qt::Horizontal);
        contrastSlider.setRange(-100, 100);
        contrastSlider.setValue(0);

        QSpinBox widthSpinBox;
        widthSpinBox.setRange(1, 10000);
        widthSpinBox.setValue(currentImage.cols);

        QSpinBox heightSpinBox;
        heightSpinBox.setRange(1, 10000);
        heightSpinBox.setValue(currentImage.rows);

        QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        // 布局
        QFormLayout layout(&dialog);
        layout.addRow("亮度:", &brightnessSlider);
        layout.addRow("对比度:", &contrastSlider);
        layout.addRow("宽度:", &widthSpinBox);
        layout.addRow("高度:", &heightSpinBox);
        layout.addRow(&buttonBox);

        // 执行对话框
        if (dialog.exec() == QDialog::Accepted) {
            // 保存当前状态用于撤销
            saveStateForUndo();

            // 调整亮度和对比度
            double alpha = 1.0 + contrastSlider.value() / 100.0;
            double beta = brightnessSlider.value();

            Mat adjusted;
            currentImage.convertTo(adjusted, -1, alpha, beta);

            // 调整大小
            int newWidth = widthSpinBox.value();
            int newHeight = heightSpinBox.value();
            if (newWidth != currentImage.cols || newHeight != currentImage.rows) {
                cv::resize(adjusted, adjusted, Size(newWidth, newHeight), 0, 0, INTER_LINEAR);
            }

            currentImage = adjusted.clone();
            displayImage(currentImage);
            statusBar()->showMessage("已调整图像参数", 1500);

            // 更新状态栏信息
            updateStatusBar();
        }
    }

    // 视图操作
    void zoomIn() {
        if (!isImageLoaded) return;
        zoomFactor *= 1.2;
        zoomSlider->setValue(static_cast<int>(zoomFactor * 100));
        displayImage(currentImage);
    }

    void zoomOut() {
        if (!isImageLoaded) return;
        zoomFactor /= 1.2;
        if (zoomFactor < 0.1) zoomFactor = 0.1;
        zoomSlider->setValue(static_cast<int>(zoomFactor * 100));
        displayImage(currentImage);
    }

    void zoomToActualSize() {
        if (!isImageLoaded) return;
        zoomFactor = 1.0;
        zoomSlider->setValue(100);
        displayImage(currentImage);
    }

    void zoomToFit() {
        if (!isImageLoaded) return;

        // 计算适合窗口的缩放比例
        double widthRatio = static_cast<double>(scrollArea->width()) / currentImage.cols;
        double heightRatio = static_cast<double>(scrollArea->height()) / currentImage.rows;
        zoomFactor = qMin(widthRatio, heightRatio) * 0.95;

        zoomSlider->setValue(static_cast<int>(zoomFactor * 100));
        displayImage(currentImage);
    }

    void rotateImage() {
        if (!isImageLoaded) return;

        // 保存当前状态用于撤销
        saveStateForUndo();

        // 旋转90度
        cv::rotate(currentImage, currentImage, ROTATE_90_CLOCKWISE);
        displayImage(currentImage);
        statusBar()->showMessage("图像已旋转90度", 1500);

        // 更新状态栏信息
        updateStatusBar();
    }

    void flipImage() {
        if (!isImageLoaded) return;

        // 保存当前状态用于撤销
        saveStateForUndo();

        // 水平翻转
        cv::flip(currentImage, currentImage, 1);
        displayImage(currentImage);
        statusBar()->showMessage("图像已水平翻转", 1500);
    }

    void cropImage() {
        if (!isImageLoaded) return;

        // 在实际应用中，这里应该实现交互式裁剪
        // 为简化示例，我们裁剪中心区域
        saveStateForUndo();

        int width = currentImage.cols;
        int height = currentImage.rows;
        int cropSize = qMin(width, height) * 0.8;
        Rect roi((width - cropSize) / 2, (height - cropSize) / 2, cropSize, cropSize);
        currentImage = currentImage(roi).clone();

        displayImage(currentImage);
        statusBar()->showMessage("图像已裁剪", 1500);

        // 更新状态栏信息
        updateStatusBar();
    }

    // 处理操作 - 二值化
    void binarize(bool inverse) {
        if (!isImageLoaded) return;

        // 保存当前状态用于撤销
        saveStateForUndo();

        Mat gray, binary;
        cvtColor(currentImage, gray, COLOR_BGR2GRAY);

        if (inverse) {
            threshold(gray, binary, 128, 255, THRESH_BINARY_INV);
        }
        else {
            threshold(gray, binary, 128, 255, THRESH_BINARY);
        }

        cvtColor(binary, currentImage, COLOR_GRAY2BGR);
        displayImage(currentImage);
        statusBar()->showMessage(inverse ? "反二值化完成" : "二值化完成", 1500);
    }

    // 灰度化
    void grayscale() {
        if (!isImageLoaded) return;

        saveStateForUndo();

        Mat gray;
        cvtColor(currentImage, gray, COLOR_BGR2GRAY);
        cvtColor(gray, currentImage, COLOR_GRAY2BGR);

        displayImage(currentImage);
        statusBar()->showMessage("灰度化完成", 1500);
    }

    // 中值滤波
    void medianFilter(int size) {
        if (!isImageLoaded) return;

        saveStateForUndo();

        Mat filtered;
        medianBlur(currentImage, filtered, size);
        currentImage = filtered.clone();

        displayImage(currentImage);
        statusBar()->showMessage(QString("中值滤波(%1x%1)完成").arg(size), 1500);
    }

    // 边缘检测
    void edgeDetection(const QString& method) {
        if (!isImageLoaded) return;

        saveStateForUndo();

        Mat gray, edges;
        cvtColor(currentImage, gray, COLOR_BGR2GRAY);

        if (method == "Sobel") {
            Mat grad_x, grad_y;
            Sobel(gray, grad_x, CV_16S, 1, 0, 3);
            Sobel(gray, grad_y, CV_16S, 0, 1, 3);
            convertScaleAbs(grad_x, grad_x);
            convertScaleAbs(grad_y, grad_y);
            addWeighted(grad_x, 0.5, grad_y, 0.5, 0, edges);
        }
        else if (method == "Canny") {
            Canny(gray, edges, 50, 150);
        }
        else if (method == "Laplacian") {
            Laplacian(gray, edges, CV_8U, 3);
        }
        else if (method == "Prewitt") {
            Mat kernelx = (Mat_<float>(3, 3) << -1, 0, 1, -1, 0, 1, -1, 0, 1);
            Mat kernely = (Mat_<float>(3, 3) << -1, -1, -1, 0, 0, 0, 1, 1, 1);
            Mat prewitt_x, prewitt_y;
            filter2D(gray, prewitt_x, CV_32F, kernelx);
            filter2D(gray, prewitt_y, CV_32F, kernely);
            convertScaleAbs(prewitt_x, prewitt_x);
            convertScaleAbs(prewitt_y, prewitt_y);
            addWeighted(prewitt_x, 0.5, prewitt_y, 0.5, 0, edges);
        }
        else { // LoG
            GaussianBlur(gray, gray, Size(3, 3), 0);
            Laplacian(gray, edges, CV_8U, 3);
        }

        cvtColor(edges, currentImage, COLOR_GRAY2BGR);
        displayImage(currentImage);
        statusBar()->showMessage(method + "边缘检测完成", 1500);
    }

    // 形态学操作
    void morphologicalOperation(const QString& operation) {
        if (!isImageLoaded) return;

        saveStateForUndo();

        Mat gray, result;
        cvtColor(currentImage, gray, COLOR_BGR2GRAY);
        Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));

        if (operation == "Dilation") {
            dilate(gray, result, kernel);
        }
        else if (operation == "Erosion") {
            erode(gray, result, kernel);
        }
        else if (operation == "Opening") {
            morphologyEx(gray, result, MORPH_OPEN, kernel);
        }
        else { // Closing
            morphologyEx(gray, result, MORPH_CLOSE, kernel);
        }

        cvtColor(result, currentImage, COLOR_GRAY2BGR);
        displayImage(currentImage);
        statusBar()->showMessage(operation + "操作完成", 1500);
    }

    // 直方图均衡化
    void histogramEqualization() {
        if (!isImageLoaded) return;

        saveStateForUndo();

        Mat ycrcb;
        cvtColor(currentImage, ycrcb, COLOR_BGR2YCrCb);

        std::vector<Mat> channels;
        split(ycrcb, channels);

        equalizeHist(channels[0], channels[0]);

        merge(channels, ycrcb);
        cvtColor(ycrcb, currentImage, COLOR_YCrCb2BGR);

        displayImage(currentImage);
        statusBar()->showMessage("直方图均衡化完成", 1500);
    }

    // 缩放滑块值改变
    void zoomSliderChanged(int value) {
        if (!isImageLoaded) return;

        zoomFactor = value / 100.0;
        if (zoomFactor < 0.1) zoomFactor = 0.1;
        if (zoomFactor > 5.0) zoomFactor = 5.0;

        displayImage(currentImage);
    }

private:
    QString filepath;
    void setupUI() {
        // 创建主窗口
        setWindowTitle("数字图像处理应用");
        resize(1000, 700);

        // 创建工具栏
        QToolBar* toolBar = new QToolBar("主工具栏", this);
        toolBar->setMovable(false);
        addToolBar(Qt::TopToolBarArea, toolBar);

        // 添加工具栏按钮
        toolBar->addAction(openAction);
        toolBar->addAction(saveAction);
        toolBar->addSeparator();
        toolBar->addAction(undoAction);
        toolBar->addSeparator();
        toolBar->addAction(zoomInAction);
        toolBar->addAction(zoomOutAction);
        toolBar->addAction(zoomFitAction);
        toolBar->addAction(zoomActualAction);
        toolBar->addAction(detectPeopleAction);

        // 创建图像显示区域
        imageLabel = new QLabel(this);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setBackgroundRole(QPalette::Dark);
        imageLabel->setAutoFillBackground(true);
        imageLabel->setText("无图像");
        imageLabel->setMinimumSize(640, 480);
        imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        imageLabel->setScaledContents(false);

        // 添加滚动区域
        scrollArea = new QScrollArea(this);
        scrollArea->setBackgroundRole(QPalette::Dark);
        scrollArea->setWidget(imageLabel);
        scrollArea->setWidgetResizable(true);
        setCentralWidget(scrollArea);

        // 创建菜单栏
        QMenuBar* menuBar = new QMenuBar(this);
        setMenuBar(menuBar);

        // 文件菜单
        QMenu* fileMenu = menuBar->addMenu("文件");
        openAction = fileMenu->addAction(QIcon(":/icons/open.png"), "打开");
        saveAction = fileMenu->addAction(QIcon(":/icons/save.png"), "保存");
        closeAction = fileMenu->addAction("关闭");
        fileMenu->addSeparator();
        exitAction = fileMenu->addAction("退出");

        // 编辑菜单
        QMenu* editMenu = menuBar->addMenu("编辑");
        undoAction = editMenu->addAction(QIcon(":/icons/undo.png"), "撤销");
        parametersAction = editMenu->addAction("图片参数");

        // 视图菜单
        QMenu* viewMenu = menuBar->addMenu("视图");
        zoomInAction = viewMenu->addAction(QIcon(":/icons/zoom-in.png"), "放大");
        zoomOutAction = viewMenu->addAction(QIcon(":/icons/zoom-out.png"), "缩小");
        zoomFitAction = viewMenu->addAction("适应窗口");
        zoomActualAction = viewMenu->addAction("实际大小");
        viewMenu->addSeparator();
        rotateAction = viewMenu->addAction("旋转");
        cropAction = viewMenu->addAction("裁剪");
        flipAction = viewMenu->addAction("翻转");

        // 处理菜单
        QMenu* processMenu = menuBar->addMenu("处理");

		//行人检测菜单
		detectPeopleAction = processMenu->addAction("检测行人");

        // 二值化子菜单
        QMenu* binarizationMenu = processMenu->addMenu("二值化");
        binarizeAction = binarizationMenu->addAction("二值化");
        inverseBinarizeAction = binarizationMenu->addAction("反二值化");

        // 灰度化
        grayscaleAction = processMenu->addAction("灰度化");

        // 去噪子菜单
        QMenu* denoiseMenu = processMenu->addMenu("去噪");
        median3x3Action = denoiseMenu->addAction("3x3中值滤波");
        median5x5Action = denoiseMenu->addAction("5x5中值滤波");

        // 边缘检测子菜单
        QMenu* edgeMenu = processMenu->addMenu("边缘检测");
        sobelAction = edgeMenu->addAction("Sobel");
        cannyAction = edgeMenu->addAction("Canny");
        laplacianAction = edgeMenu->addAction("Laplacian");
        prewittAction = edgeMenu->addAction("Prewitt");
        logAction = edgeMenu->addAction("LoG");

        // 形态学操作子菜单
        QMenu* morphMenu = processMenu->addMenu("形态学操作");
        dilationAction = morphMenu->addAction("膨胀");
        erosionAction = morphMenu->addAction("腐蚀");
        openingAction = morphMenu->addAction("开运算");
        closingAction = morphMenu->addAction("闭运算");

        // 直方图操作
        QMenu* histMenu = processMenu->addMenu("直方图");
        histEqualizationAction = histMenu->addAction("均衡化");
    }

    void setupStatusBar() {
        // 创建状态栏
        QStatusBar* statusBar = this->statusBar();

        // 添加缩放控制
        zoomLabel = new QLabel("缩放:", statusBar);
        statusBar->addPermanentWidget(zoomLabel);

        zoomSlider = new QSlider(Qt::Horizontal, statusBar);
        zoomSlider->setRange(10, 500); // 10% 到 500%
        zoomSlider->setValue(100);
        zoomSlider->setFixedWidth(150);
        zoomSlider->setToolTip("调整图像缩放比例");
        statusBar->addPermanentWidget(zoomSlider);

        zoomValueLabel = new QLabel("100%", statusBar);
        zoomValueLabel->setFixedWidth(50);
        statusBar->addPermanentWidget(zoomValueLabel);

        // 添加分隔符
        statusBar->addPermanentWidget(new QLabel(" | ", statusBar));

        // 添加图像信息
        imageInfoLabel = new QLabel("图像: 无", statusBar);
        imageInfoLabel->setMinimumWidth(200);
        statusBar->addPermanentWidget(imageInfoLabel);

        // 添加分隔符
        statusBar->addPermanentWidget(new QLabel(" | ", statusBar));

        // 添加操作信息
        operationLabel = new QLabel("就绪", statusBar);
        operationLabel->setMinimumWidth(200);
        statusBar->addWidget(operationLabel);

        // 连接缩放滑块
        connect(zoomSlider, &QSlider::valueChanged, this, &ImageProcessor::zoomSliderChanged);
        connect(zoomSlider, &QSlider::valueChanged, this, [this](int value) {
            zoomValueLabel->setText(QString("%1%").arg(value));
            });
    }

    void connectActions() {
        // 文件操作
        connect(openAction, &QAction::triggered, this, &ImageProcessor::openImage);
        connect(saveAction, &QAction::triggered, this, &ImageProcessor::saveImage);
        connect(closeAction, &QAction::triggered, this, &ImageProcessor::closeImage);
        connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

        // 编辑操作
        connect(undoAction, &QAction::triggered, this, &ImageProcessor::undo);
        connect(parametersAction, &QAction::triggered, this, &ImageProcessor::adjustParameters);

        // 视图操作
        connect(zoomInAction, &QAction::triggered, this, &ImageProcessor::zoomIn);
        connect(zoomOutAction, &QAction::triggered, this, &ImageProcessor::zoomOut);
        connect(zoomFitAction, &QAction::triggered, this, &ImageProcessor::zoomToFit);
        connect(zoomActualAction, &QAction::triggered, this, &ImageProcessor::zoomToActualSize);
        connect(rotateAction, &QAction::triggered, this, &ImageProcessor::rotateImage);
        connect(flipAction, &QAction::triggered, this, &ImageProcessor::flipImage);
        connect(cropAction, &QAction::triggered, this, &ImageProcessor::cropImage);

        // 处理操作
        connect(binarizeAction, &QAction::triggered, this, [this]() { binarize(false); });
        connect(inverseBinarizeAction, &QAction::triggered, this, [this]() { binarize(true); });
        connect(grayscaleAction, &QAction::triggered, this, &ImageProcessor::grayscale);
        connect(median3x3Action, &QAction::triggered, this, [this]() { medianFilter(3); });
        connect(median5x5Action, &QAction::triggered, this, [this]() { medianFilter(5); });
        connect(sobelAction, &QAction::triggered, this, [this]() { edgeDetection("Sobel"); });
        connect(cannyAction, &QAction::triggered, this, [this]() { edgeDetection("Canny"); });
        connect(laplacianAction, &QAction::triggered, this, [this]() { edgeDetection("Laplacian"); });
        connect(prewittAction, &QAction::triggered, this, [this]() { edgeDetection("Prewitt"); });
        connect(logAction, &QAction::triggered, this, [this]() { edgeDetection("LoG"); });
        connect(dilationAction, &QAction::triggered, this, [this]() { morphologicalOperation("Dilation"); });
        connect(erosionAction, &QAction::triggered, this, [this]() { morphologicalOperation("Erosion"); });
        connect(openingAction, &QAction::triggered, this, [this]() { morphologicalOperation("Opening"); });
        connect(closingAction, &QAction::triggered, this, [this]() { morphologicalOperation("Closing"); });
        connect(histEqualizationAction, &QAction::triggered, this, &ImageProcessor::histogramEqualization);
        connect(detectPeopleAction, &QAction::triggered, this, &ImageProcessor::detect);
    }



    // 显示图像
    void displayImage(const Mat& image) {
        if (image.empty()) return;

        // 应用缩放
        Mat displayMat;
        if (zoomFactor != 1.0) {
            cv::resize(image, displayMat, Size(), zoomFactor, zoomFactor, INTER_LINEAR);
        }
        else {
            displayMat = image.clone();
        }

        // 将OpenCV Mat转换为QImage
        Mat rgb;
        cvtColor(displayMat, rgb, COLOR_BGR2RGB);
        QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);

        // 显示图像
        imageLabel->setPixmap(QPixmap::fromImage(qimg));
        imageLabel->adjustSize();
    }

    // 更新状态栏信息
    void updateStatusBar() {
        if (!isImageLoaded) {
            imageInfoLabel->setText("图像: 无");
            return;
        }

        QString info = QString("图像: %1x%2 | 通道: %3 | 深度: %4")
            .arg(currentImage.cols)
            .arg(currentImage.rows)
            .arg(currentImage.channels())
            .arg(currentImage.depth());

        imageInfoLabel->setText(info);
    }

    // 保存当前状态用于撤销
    void saveStateForUndo() {
        undoStack.push(currentImage.clone());
    }



    // 自定义撤销栈类
    class UndoStack : public QStack<Mat> {
    public:
        void setMaxCount(int count) {
            maxCount = count;
        }

        void push(const Mat& image) {
            // 如果栈已满，移除最旧的元素
            if (count() >= maxCount) {
                removeFirst();
            }
            QStack<Mat>::push(image);
        }

    private:
        int maxCount;
    };

private:
    // UI元素
    QLabel* imageLabel;
    QScrollArea* scrollArea;

    // 状态栏控件
    QSlider* zoomSlider;
    QLabel* zoomLabel;
    QLabel* zoomValueLabel;
    QLabel* imageInfoLabel;
    QLabel* operationLabel;

    // 菜单动作
    QAction* openAction;
    QAction* saveAction;
    QAction* closeAction;
    QAction* exitAction;
    QAction* undoAction;
    QAction* parametersAction;
    QAction* zoomInAction;
    QAction* zoomOutAction;
    QAction* zoomFitAction;
    QAction* zoomActualAction;
    QAction* rotateAction;
    QAction* cropAction;
    QAction* flipAction;
    QAction* binarizeAction;
    QAction* inverseBinarizeAction;
    QAction* grayscaleAction;
    QAction* median3x3Action;
    QAction* median5x5Action;
    QAction* sobelAction;
    QAction* cannyAction;
    QAction* laplacianAction;
    QAction* prewittAction;
    QAction* logAction;
    QAction* dilationAction;
    QAction* erosionAction;
    QAction* openingAction;
    QAction* closingAction;
    QAction* histEqualizationAction;
	QAction* detectPeopleAction;

    // 图像数据
    Mat currentImage;
    Mat originalImage;
    bool isImageLoaded;
    double zoomFactor;

    // 撤销栈
    UndoStack undoStack;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 设置应用样式
    app.setStyle("Fusion");

    // 创建主窗口
    ImageProcessor window;
    window.show();

    return app.exec();
}

#include "main.moc"

