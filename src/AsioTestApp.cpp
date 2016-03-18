#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/detail/thread_group.hpp>
#include <boost/bind.hpp>
#include <iostream>

using namespace ci;
using namespace ci::app;
using namespace std;

boost::mutex global_stream_lock; // so the output stream

class AsioTestApp : public App {
  public:
    ~AsioTestApp();
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
    void WorkerThread(std::shared_ptr< boost::asio::io_service > _io_service);
    void TimerHandlerTS( const boost::system::error_code & error,	std::shared_ptr< boost::asio::deadline_timer > timer, 	std::shared_ptr< boost::asio::io_service::strand > strand);
    void TimerHandler( const boost::system::error_code & error,	std::shared_ptr< boost::asio::deadline_timer > timer);
    void PrintNum(int x);
    std::vector<std::thread> mThreads;
    std::shared_ptr< boost::asio::io_service > io_service;
    std::shared_ptr< boost::asio::io_service::work > work;
    std::shared_ptr< boost::asio::io_service::strand > strand;
};

void AsioTestApp::PrintNum( int x )
{
    std::cout << "[" << std::this_thread::get_id()
    << "] x: " << x << std::endl;
}

void AsioTestApp::WorkerThread( std::shared_ptr< boost::asio::io_service > _io_service )
{
    global_stream_lock.lock();
    std::cout << "[" << std::this_thread::get_id()
    << "] Thread Start" << std::endl;
    global_stream_lock.unlock();
    
    while( true )
    {
        try
        {
            boost::system::error_code ec;
            _io_service->run( ec );
            if( ec )
            {
                global_stream_lock.lock();
                std::cout << "[" << boost::this_thread::get_id()
                << "] Error: " << ec << std::endl;
                global_stream_lock.unlock();
            }
            break;
        }
        catch( std::exception & ex )
        {
            global_stream_lock.lock();
            std::cout << "[" << boost::this_thread::get_id()
            << "] Exception: " << ex.what() << std::endl;
            global_stream_lock.unlock();
        }
    }

    
    global_stream_lock.lock();
    std::cout << "[" << std::this_thread::get_id()
    << "] Thread Finish" << std::endl;
    global_stream_lock.unlock();
}

void AsioTestApp::TimerHandlerTS( const boost::system::error_code & error, std::shared_ptr< boost::asio::deadline_timer > timer, std::shared_ptr< boost::asio::io_service::strand > strand)
{
    if( error )
    {
        global_stream_lock.lock();
        std::cout << "[" << boost::this_thread::get_id()
        << "] Error: " << error << std::endl;
        global_stream_lock.unlock();
    }
    else
    {
        global_stream_lock.lock();
        std::cout << "[" << boost::this_thread::get_id()
        << "] TimerHandler " << std::endl;
        global_stream_lock.unlock();
        
        strand->post(std::bind(&AsioTestApp::PrintNum, this, 99));
        timer->expires_from_now( boost::posix_time::seconds( 5 ) );
        timer->async_wait(strand->wrap( std::bind(&AsioTestApp::TimerHandlerTS,this,std::placeholders::_1, timer, strand)));
    }
}

void AsioTestApp::TimerHandler( const boost::system::error_code & error, std::shared_ptr< boost::asio::deadline_timer > timer)
{
    if( error )
    {
        global_stream_lock.lock();
        std::cout << "[" << boost::this_thread::get_id()
        << "] Error: " << error << std::endl;
        global_stream_lock.unlock();
    }
    else
    {
        global_stream_lock.lock();
        std::cout << "[" << boost::this_thread::get_id()
        << "] TimerHandler " << std::endl;
        global_stream_lock.unlock();
        
        io_service->post(std::bind(&AsioTestApp::PrintNum, this, 99));

        timer->expires_from_now( boost::posix_time::seconds( 5 ) );
        timer->async_wait( std::bind(&AsioTestApp::TimerHandler,this,std::placeholders::_1, timer));
    }
}

void AsioTestApp::setup()
{
    io_service = std::shared_ptr< boost::asio::io_service >(new boost::asio::io_service);
    work = std::shared_ptr< boost::asio::io_service::work >(new boost::asio::io_service::work( *io_service ));
    strand = std::shared_ptr< boost::asio::io_service::strand > (new boost::asio::io_service::strand( *io_service ));

    
    global_stream_lock.lock();
    std::cout << "[" <<  boost::this_thread::get_id()
    << "] The program will exit when all  work has finished." <<  std::endl;
    global_stream_lock.unlock();

    
    //push back a few threads with workers waiting
    for( int x = 0; x < 2; ++x ){
        mThreads.push_back(std::thread( std::bind( &AsioTestApp::WorkerThread,this, io_service ) ));
    }
    
    //aynchronous timers
//        std::shared_ptr< boost::asio::deadline_timer > timer(new boost::asio::deadline_timer( *io_service ));
//        timer->expires_from_now( boost::posix_time::seconds( 5 ) );
//        timer->async_wait( std::bind( &AsioTestApp::TimerHandler,this, std::placeholders::_1, timer) );
//
    
//       thread safe asychronous timers using strand- ie if workers and timers need to access same shared object use this
        std::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer( *io_service) );
        timer->expires_from_now( boost::posix_time::seconds( 5 ) );
        timer->async_wait(strand->wrap(std::bind(&AsioTestApp::TimerHandlerTS,this,std::placeholders::_1, timer, strand)));
    

    
    
    sleep(1);
    //guaranteed to not happen concurrently ie threadsafe,  a single strand is synchronous across multiple threads
    strand->post( std::bind( &AsioTestApp::PrintNum,this, 1 ) );
    strand->post( std::bind( &AsioTestApp::PrintNum,this, 2 ) );
    strand->post( std::bind( &AsioTestApp::PrintNum,this, 3 ) );
    strand->post( std::bind( &AsioTestApp::PrintNum,this, 4 ) );
    strand->post( std::bind( &AsioTestApp::PrintNum,this, 5 ) );
    
    sleep(1);
//    //otherwise if concurrency is not an issue
    io_service->post(std::bind(&AsioTestApp::PrintNum, this, 6));
    io_service->post(std::bind(&AsioTestApp::PrintNum, this, 7));
    io_service->post(std::bind(&AsioTestApp::PrintNum, this, 8));
    io_service->post(std::bind(&AsioTestApp::PrintNum, this, 9));
    io_service->post(std::bind(&AsioTestApp::PrintNum, this, 10));
    
    sleep(1);
    //still trying to figure out the difference between dispatch and post
    // if called by io_service, it will be called immediately
    // otherwise it will queue
    //if functions are reentrant, and they need to be,  faster
    io_service->dispatch(std::bind(&AsioTestApp::PrintNum, this, 11));
    io_service->dispatch(std::bind(&AsioTestApp::PrintNum, this, 12));
    io_service->dispatch(std::bind(&AsioTestApp::PrintNum, this, 13));
    io_service->dispatch(std::bind(&AsioTestApp::PrintNum, this, 14));
    io_service->dispatch(std::bind(&AsioTestApp::PrintNum, this, 15));
 
}

AsioTestApp::~AsioTestApp(){
    work.reset();
    io_service->stop();
    for(auto &m : mThreads){
        m.join();
    }
}

void AsioTestApp::mouseDown( MouseEvent event )
{
    io_service->dispatch( std::bind( &AsioTestApp::PrintNum,this, 77 ) );

}

void AsioTestApp::update()
{
}

void AsioTestApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( AsioTestApp, RendererGl )
