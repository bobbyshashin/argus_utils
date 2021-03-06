#include <extrinsics_array/ExtrinsicsInterface.h>
#include <argus_utils/utils/ParamUtils.h>
#include <argus_utils/geometry/GeometryUtils.h>

namespace argus
{

ExtrinsicsInterface::ExtrinsicsInterface( const ros::NodeHandle& nh )
{
	SetMaxCacheTime( 10.0 );
	_tfListener = std::make_shared<tf2_ros::TransformListener>( *_tfBuffer, nh );
}

ExtrinsicsInterface::ExtrinsicsInterface( const ros::NodeHandle& nh,
                                          const ros::NodeHandle& ph )
{
	ReadParams( ph );
	_tfListener = std::make_shared<tf2_ros::TransformListener>( *_tfBuffer, nh );
}

void ExtrinsicsInterface::ReadParams( const ros::NodeHandle& ph )
{
	double t;
	GetParam( ph, "max_cache_time", t, 10.0 );
	SetMaxCacheTime( t );
}

void ExtrinsicsInterface::SetMaxCacheTime( double t )
{
	_tfBuffer = std::make_shared<tf2_ros::Buffer>( ros::Duration( t ) );
}

void ExtrinsicsInterface::SetExtrinsics( const std::string& from,
                                         const std::string& to,
                                         const ros::Time& stamp,
                                         const PoseSE3& pose )
{
	RelativePose rpose( to, from, pose, stamp );
	return SetExtrinsics( rpose );
}

void ExtrinsicsInterface::SetExtrinsics( const RelativePose& pose )
{
	geometry_msgs::TransformStamped msg;
	msg = pose.ToTransformMsg();
	_tfBroadcaster.sendTransform( msg );
}

void ExtrinsicsInterface::SetStaticExtrinsics( const std::string& from,
                                               const std::string& to,
                                               const PoseSE3& pose )
{
	RelativePose rpose( to, from, pose );
	return SetStaticExtrinsics( rpose );
}

void ExtrinsicsInterface::SetStaticExtrinsics( const RelativePose& pose )
{
	geometry_msgs::TransformStamped msg;
	msg = pose.ToTransformMsg();
	_tfStaticBroadcaster.sendTransform( msg );
}

PoseSE3 ExtrinsicsInterface::Convert( std::string fromIn,
                                      std::string toIn,
                                      const ros::Time& timeIn,
                                      const PoseSE3& poseIn,
                                      std::string fromOut,
                                      std::string toOut )
{
	fromIn = Sanitize( fromIn );
	toIn = Sanitize( toIn );
	fromOut = Sanitize( fromOut );
	toOut = Sanitize( toOut );
	try
	{
		PoseSE3 fromExt, parentExt;
		if( fromIn != fromOut ) { fromExt = GetExtrinsics( fromOut, fromIn, timeIn ); }
		if( toIn != toOut ) { parentExt = GetExtrinsics( toIn, toOut, timeIn ); }
		return parentExt * poseIn * fromExt;
	}
	catch( ExtrinsicsException ex )
	{}

	try
	{
		PoseSE3 fromExt, parentExt;
		if( fromOut != toIn ) { fromExt = GetExtrinsics( fromOut, toIn, timeIn ); }
		if( fromIn != toOut ) { parentExt = GetExtrinsics( fromIn, toOut, timeIn ); }
		return parentExt * poseIn.Inverse() * fromExt;
	}
	catch( ExtrinsicsException ex )
	{
		throw ExtrinsicsException( "Could not convert " + fromIn + " -> " + toIn +
		                           " to requested " + fromOut + " -> " + toOut );
	}
}

PoseSE3 ExtrinsicsInterface::GetExtrinsics( std::string from,
                                            std::string to,
                                            const ros::Time& time )
{
	return GetExtrinsics( from, time, to, time );
}

PoseSE3 ExtrinsicsInterface::GetDisplacement( std::string from,
                                              const ros::Time& start,
                                              const ros::Time& stop )
{
	return GetExtrinsics( from, start, from, stop );
}

PoseSE3 ExtrinsicsInterface::GetExtrinsics( std::string from,
                                            const ros::Time& fromTime,
                                            std::string to,
                                            const ros::Time& toTime )
{
	if( from.empty() || to.empty() )
	{
		throw ExtrinsicsException( "Frames " + from + ", " + to + " cannot be empty!" );
	}
	
	from = Sanitize( from );
	to = Sanitize( to );
	std::string err;
	// NOTE Assuming the transform should be static in the to frame
	if( !_tfBuffer->canTransform( from, fromTime,
	                              to, toTime,
	                              to, ros::Duration( 0 ),
	                              &err ) )
	{
		throw ExtrinsicsException( "Could get extrinsics of " + from + " to " +
		                           to + " due to: " + err );
	}
	geometry_msgs::TransformStamped msg = _tfBuffer->lookupTransform( to,
	                                                                  toTime,
	                                                                  from,
	                                                                  fromTime,
	                                                                  to,
	                                                                  ros::Duration( 0 ) );
	return TransformToPose( msg.transform );
}

std::string ExtrinsicsInterface::Sanitize( std::string in )
{
	if( in.front() == '/' )
	{
		in.erase( 0, 1 );
	}
	return in;
}

}
