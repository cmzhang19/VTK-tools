//拾取交互
#include <vtkPropPicker.h>
#include <vtkSphereSource.h>
#include <vtkNamedColors.h>
#include <vtkRendererCollection.h>
#include <vtkObjectFactory.h>
#include <vtkPointSource.h>
#include <vtkXYPlotActor.h>
#include <vtkContextActor.h>
#include <vtkPen.h>
#include <vtkContextScene.h>
#include <vtkRenderer.h>
#include <vtkAxisActor.h>
#include <vtkTextProperty.h>

using namespace std;
using namespace cv;

//交互class

namespace {
// Handle mouse events
class MouseInteractorStyle2 : public vtkInteractorStyleImage
{
protected:

    vtkNew<vtkImageData> imageA;
    vtkNew<vtkImageData> imageB;
    vtkRenderer* cRenderer;
    vtkNew<vtkContextActor> fittingplot;
    vtkNew<vtkContextView> View;
    vtkRenderWindow* Window;
    vector<vtkSmartPointer<vtkImageData>> Original_pics;

public:

  double pixel_x;
  double pixel_y;

  static MouseInteractorStyle2* New();
  vtkTypeMacro(MouseInteractorStyle2, vtkInteractorStyleImage)
  vtkNew<vtkNamedColors> colors;

  void setimageA(vtkImageData* image)
  {
      imageA->DeepCopy(image);
  }

  void setimageB(vtkImageData* image)
  {
      imageB->DeepCopy(image);
  }

  void fixCurrentRenderer(vtkRenderer* render)
  {
      cRenderer = render;
  }

  void setRenderWindow(vtkRenderWindow* window)
  {
      Window = window;
  }

  void importPicSeries(vector<vtkSmartPointer<vtkImageData>> original_pics)
  {
      Original_pics = original_pics;
  }

  virtual void OnMouseWheelForward() override //取消滚轮交互功能
  {
  }
  virtual void OnMouseWheelBackward() override //取消滚轮交互功能
  {
  }
  virtual void OnMouseMove() override //取消鼠标左键按下后滑动调整对比度
  {
  }

  //vtkRenderer* crenderer = this->GetCurrentRenderer();

  virtual void OnLeftButtonDown() override
  {
    //debug用current看一下
    //auto crenderer = this->GetCurrentRenderer();
    //cout << crenderer <<endl;

    int* clickPos = this->GetInteractor()->GetEventPosition();

    // Pick from this location.
    vtkNew<vtkPropPicker> picker;
    picker->Pick(clickPos[0], clickPos[1], 0, this->GetDefaultRenderer());

    double* pos = picker->GetPickPosition();
    //std::cout << "Pick position (world coordinates) is: " << pos[0] << " "
    //          << pos[1] << " " << pos[2] << std::endl;

    this->pixel_x = floor(pos[0]);
    this->pixel_y = floor(pos[1]);

    auto pickedActor = picker->GetProp3D();

    if (pickedActor == nullptr)
    {
      std::cout << "No actor picked." << std::endl;
    }

    else
    {
      //std::cout << "Picked actor: " << picker->GetProp3D() << std::endl;
      // Create a sphere
      vtkNew<vtkPointSource> pointSource;
      pointSource->SetCenter(pos[0], pos[1], 0);
      //pointSource->SetNumberOfPoints(1);
      //pointSource->SetRadius(10);

      // Create a mapper and actor
      vtkNew<vtkPolyDataMapper> mapper;
      mapper->SetInputConnection(pointSource->GetOutputPort());

      vtkNew<vtkActor> actor;
      actor->SetMapper(mapper);
      actor->GetProperty()->SetPointSize(6);
      actor->GetProperty()->SetColor(colors->GetColor3d("Black").GetData());

      //取消之前选取的点
      vtkActorCollection* actorCollection = this->GetDefaultRenderer()->GetActors();
      int num = actorCollection->GetNumberOfItems();
      //这个函数比较重要，否则getNextActor将没法得到正确的actor
      actorCollection->InitTraversal();
      for (int i=0;i<num;++i)
      {
      vtkActor* actor = actorCollection->GetLastActor();
      this->GetDefaultRenderer()->RemoveActor(actor);
      }

      this->GetDefaultRenderer()->AddActor(actor);

      std::cout << "Pixel coordinates is: " << pixel_x << " "
                << pixel_y << " " << std::endl;

      //Creat table 这里实现拟合曲线的展示
      vtkNew<vtkTable> table;
      vtkNew<vtkFloatArray> timePoints;
      timePoints->SetName("Time(s)");
      table->AddColumn(timePoints);
      vtkNew<vtkFloatArray> fittingCurve;
      fittingCurve->SetName("Fitting Curve");
      table->AddColumn(fittingCurve);
      vtkNew<vtkFloatArray> originalPixel;
      originalPixel->SetName("Original Value");
      table->AddColumn(originalPixel);

      //Fill in the table with values
      size_t numPoints = 14;
      table->SetNumberOfRows(static_cast<vtkIdType>(numPoints));

      double* pA = static_cast<double*>(imageA->GetScalarPointer(pixel_x, pixel_y, 0));
      double* pB = static_cast<double*>(imageB->GetScalarPointer(pixel_x, pixel_y, 0));
      double A = *pA;
      double B = *pB;
      cout << A << " " << B << endl;

      for(size_t i=0; i<numPoints; ++i)
      {
          double t = 0 + 0.05*i;
          table->SetValue(static_cast<vtkIdType>(i), 0, t);
          table->SetValue(static_cast<vtkIdType>(i), 1, A*(1-exp(-B*t)));
      }

      for(size_t i=0; i<numPoints; ++i)
      {
          vtkNew<vtkImageData> image_decouple;
          image_decouple->DeepCopy(Original_pics[i]);
          double* pPix = static_cast<double*>(image_decouple->GetScalarPointer(pixel_x, pixel_y, 0));
          double Pix = *pPix;
          table->SetValue(static_cast<vtkIdType>(i), 2, Pix);
      }

      vtkNew<vtkChartXY> chart;
      chart->DrawAxesAtOriginOn();
      chart->SetAutoSize(false);
      chart->SetSize(vtkRectf(40, 25, 600, 180));
      chart->SetShowLegend(true);

      //运行ok代码
      vtkPlot* line = chart->AddPlot(vtkChart::LINE);
      line->SetInputData(table,0,1);
      line->SetWidth(3.0);
      auto lineColor = colors->GetColor3d("Red").GetData();
      line->SetColor(lineColor[0], lineColor[1], lineColor[2]);
      line->GetXAxis()->SetTitle("Time(s)");
      line->GetXAxis()->GetTitleProperties()->SetColor(colors->GetColor3d("White").GetData()); //轴标题颜色
      line->GetXAxis()->GetLabelProperties()->SetColor(colors->GetColor3d("White").GetData()); //轴坐标数字颜色
      line->GetXAxis()->GetPen()->SetColor(colors->GetColor4ub("White")); //轴本身颜色
      line->GetYAxis()->SetTitle("Fitting intensity");
      line->GetYAxis()->GetTitleProperties()->SetColor(colors->GetColor3d("White").GetData());
      line->GetYAxis()->GetLabelProperties()->SetColor(colors->GetColor3d("White").GetData());
      line->GetYAxis()->GetPen()->SetColor(colors->GetColor4ub("White")); //轴本身颜色

      vtkPlot* dot = chart->AddPlot(vtkChart::POINTS);
      dot->SetInputData(table,0,2);
      auto dotColor = colors->GetColor3d("Lime").GetData();
      dot->SetColor(dotColor[0], dotColor[1], dotColor[2]);

      vtkNew<vtkContextScene> Scene;
      Scene->AddItem(chart);

      //line->GetXAxis()->SetAxisVisible(true);
      //ChartXY
      //vtkNew<vtkContextActor> fittingplot; //把这个放到外面去
      
      fittingplot->SetScene(Scene);

      this->cRenderer->AddActor(fittingplot);
      //this->cRenderer->AddActor(axis);
      // Forward events
      vtkInteractorStyleImage::OnLeftButtonDown();

    }
  }

private:

};

vtkStandardNewMacro(MouseInteractorStyle2)

} // namespace
