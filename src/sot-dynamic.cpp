#include <sot/core/debug.hh>
#include <sot-dynamic-pinocchio/dynamic.h>

#include <boost/version.hpp>
//#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <jrl/mal/matrixabstractlayer.hh>

#include <dynamic-graph/all-commands.h>

#include <pinocchio/algorithm/kinematics.hpp>
#include <pinocchio/spatial/motion.hpp>
#include <pinocchio/algorithm/crba.hpp>

#include "../src/dynamic-command.h"

using namespace dynamicgraph::sot;
using namespace dynamicgraph;

const std::string dynamicgraph::sot::Dynamic::CLASS_NAME = "DynamicLib";

using namespace std;
/* --------------------------------------------------------------------- */
/* -----------------------ENUM------------------------------------------ */
/* --------------------------------------------------------------------- */

enum CmdLine{notdef,
             debugInertia,
             setFiles,
             displayFiles,
             parse,
             createJacobian,
             destroyJacobian,
             createPosition,
             destroyPosition,
             createVelocity,
             destroyVelocity,
             createAcceleration,
             destroyAcceleration,
             createEndeffJacobian,
             createOpPoint,
             destroyOpPoint,
             ndof,
             setComputeCom,
             setComputeZmp,
             setProperty,
             getProperty,
             displayProperties,
             help
            };

static std::map<std::string, CmdLine> mapCmdLine;

void Dynamic::initMap(){
    mapCmdLine["debugInertia"] = debugInertia;
    mapCmdLine["setFiles"] = setFiles;
    mapCmdLine["displayFiles"]= displayFiles;
    mapCmdLine["parse"]= parse;
    mapCmdLine["createJacobian"]= createJacobian;
    mapCmdLine["destroyJacobian"]= destroyJacobian;
    mapCmdLine["createPosition"]= createPosition;
    mapCmdLine["destroyPosition"]= destroyPosition;
    mapCmdLine["createVelocity"]= createVelocity;
    mapCmdLine["destroyVelocity"]= destroyVelocity;
    mapCmdLine["createAcceleration"]= createAcceleration;
    mapCmdLine["destroyAcceleration"]= destroyAcceleration;
    mapCmdLine["createEndeffJacobian"]= createEndeffJacobian;
    mapCmdLine["createOpPoint"]= createOpPoint;
    mapCmdLine["destroyOpPoint"]= destroyOpPoint;
    mapCmdLine["ndof"]= ndof;
    mapCmdLine["setComputeCom"]= setComputeCom;
    mapCmdLine["setComputeZmp"]= setComputeZmp;
    mapCmdLine["setProperty"]= setProperty;
    mapCmdLine["getProperty"]= getProperty;
    mapCmdLine["displayProperties"]= displayProperties;
    mapCmdLine["help"]= help;
}


Dynamic::Dynamic( const std::string & name, bool build ):Entity(name)
  ,m_data(NULL)
  ,m_model()

  ,init(false)

  ,jointPositionSIN         (NULL,"sotDynamic("+name+")::input(vector)::position")
  ,freeFlyerPositionSIN     (NULL,"sotDynamic("+name+")::input(vector)::ffposition")
  ,jointVelocitySIN         (NULL,"sotDynamic("+name+")::input(vector)::velocity")
  ,freeFlyerVelocitySIN     (NULL,"sotDynamic("+name+")::input(vector)::ffvelocity")
  ,jointAccelerationSIN     (NULL,"sotDynamic("+name+")::input(vector)::acceleration")
  ,freeFlyerAccelerationSIN (NULL,"sotDynamic("+name+")::input(vector)::ffacceleration")
  ,newtonEulerSINTERN( boost::bind(&Dynamic::computeNewtonEuler,this,_1,_2),
                       sotNOSIGNAL,
                       "sotDynamic("+name+")::intern(dummy)::newtoneuleur" )

  ,zmpSOUT( boost::bind(&Dynamic::computeZmp,this,_1,_2),
            newtonEulerSINTERN,
            "sotDynamic("+name+")::output(vector)::zmp" )
  ,JcomSOUT( boost::bind(&Dynamic::computeJcom,this,_1,_2),
             newtonEulerSINTERN,
             "sotDynamic("+name+")::output(matrix)::Jcom" )
  ,comSOUT( boost::bind(&Dynamic::computeCom,this,_1,_2),
            newtonEulerSINTERN,
            "sotDynamic("+name+")::output(vector)::com" )
  ,inertiaSOUT( boost::bind(&Dynamic::computeInertia,this,_1,_2),
                newtonEulerSINTERN,
                "sotDynamic("+name+")::output(matrix)::inertia" )
  ,footHeightSOUT( boost::bind(&Dynamic::computeFootHeight,this,_1,_2),
                   newtonEulerSINTERN,
                   "sotDynamic("+name+")::output(double)::footHeight" )

  ,upperJlSOUT( boost::bind(&Dynamic::getUpperJointLimits,this,_1,_2),
                sotNOSIGNAL,
                "sotDynamic("+name+")::output(vector)::upperJl" )

  ,lowerJlSOUT( boost::bind(&Dynamic::getLowerJointLimits,this,_1,_2),
                sotNOSIGNAL,
                "sotDynamic("+name+")::output(vector)::lowerJl" )

  ,upperVlSOUT( boost::bind(&Dynamic::getUpperVelocityLimits,this,_1,_2),
                sotNOSIGNAL,
                "sotDynamic("+name+")::output(vector)::upperVl" )

  ,upperTlSOUT( boost::bind(&Dynamic::getUpperTorqueLimits,this,_1,_2),
                sotNOSIGNAL,
                "sotDynamic("+name+")::output(vector)::upperTl" )

  ,inertiaRotorSOUT( "sotDynamic("+name+")::output(matrix)::inertiaRotor" )
  ,gearRatioSOUT( "sotDynamic("+name+")::output(matrix)::gearRatio" )
  ,inertiaRealSOUT( boost::bind(&Dynamic::computeInertiaReal,this,_1,_2),
                    inertiaSOUT << gearRatioSOUT << inertiaRotorSOUT,
                    "sotDynamic("+name+")::output(matrix)::inertiaReal" )
  ,MomentaSOUT( boost::bind(&Dynamic::computeMomenta,this,_1,_2),
                newtonEulerSINTERN,
                "sotDynamic("+name+")::output(vector)::momenta" )
  ,AngularMomentumSOUT( boost::bind(&Dynamic::computeAngularMomentum,this,_1,_2),
                        newtonEulerSINTERN,
                        "sotDynamic("+name+")::output(vector)::angularmomentum" )
  ,dynamicDriftSOUT( boost::bind(&Dynamic::computeTorqueDrift,this,_1,_2),
                     newtonEulerSINTERN,
                     "sotDynamic("+name+")::output(vector)::dynamicDrift" )
{
    sotDEBUGIN(5);



    signalRegistration(jointPositionSIN);
    signalRegistration(freeFlyerPositionSIN);
    signalRegistration(jointVelocitySIN);
    signalRegistration(freeFlyerVelocitySIN);
    signalRegistration(jointAccelerationSIN);
    signalRegistration(freeFlyerAccelerationSIN);
    signalRegistration(zmpSOUT);
    signalRegistration(comSOUT);
    signalRegistration(JcomSOUT);
    signalRegistration(footHeightSOUT);
    signalRegistration(upperJlSOUT);
    signalRegistration(lowerJlSOUT);
    signalRegistration(upperVlSOUT);
    signalRegistration(upperTlSOUT);
    signalRegistration(inertiaSOUT);
    signalRegistration(inertiaRealSOUT);
    signalRegistration(inertiaRotorSOUT);
    signalRegistration(gearRatioSOUT);
    signalRegistration( MomentaSOUT);
    signalRegistration(AngularMomentumSOUT);
    signalRegistration(dynamicDriftSOUT);

    //
    // Commande
    //
    // #### Work in progress
    initMap();

    std::string docstring;

    // setFiles
    docstring =
            "\n"
            "    Define files to parse in order to build the robot.\n"
            "\n"
            "      Input:\n"
            "        - a string: urdf file with its path,\n"
            "\n";
    addCommand("setFiles",
               new command::SetFiles(*this, docstring));

    // parse
    docstring =
            "\n"
            "    Informe if files already parsed by setFiles\n"
            "\n"
            "      No input.\n"
            "      Files are defined by command setFiles \n"
            "\n";
    addCommand("parse",
               new command::Parse(*this, docstring));

    {
        using namespace ::dynamicgraph::command;


        // CreateOpPoint
        docstring =
                "    \n"
                "    Create an operational point attached to a robot joint local frame.\n"
                "    \n"
                "      Input: \n"
                "        - a string: name of the operational point,\n"
                "        - a string: name the joint, among (gaze, left-ankle, right ankle\n"
                "          , left-wrist, right-wrist, waist, chest).\n"
                "\n";
        addCommand("CreateOpPoint",
                   makeCommandVoid2(*this,&Dynamic::cmd_createOpPointSignals,
                                    docstring));

        docstring = docCommandVoid2("Create a jacobian (world frame) signal only for one joint.",
                                    "string (signal name)","string (joint name)");
        addCommand("createJacobian",
                   makeCommandVoid2(*this,&Dynamic::cmd_createJacobianWorldSignal,
                                    docstring));

        docstring = docCommandVoid2("Create a jacobian (endeff frame) signal only for one joint.",
                                    "string (signal name)","string (joint name)");
        addCommand("createJacobianEndEff",
                   makeCommandVoid2(*this,&Dynamic::cmd_createJacobianEndEffectorSignal,
                                    docstring));

        docstring = docCommandVoid2("Create a position (matrix homo) signal only for one joint.",
                                    "string (signal name)","string (joint name)");
        addCommand("createPosition",
                   makeCommandVoid2(*this,&Dynamic::cmd_createPositionSignal,docstring));
    }

    // SetProperty
    docstring = "    \n"
            "    Set a property.\n"
            "    \n"
            "      Input:\n"
            "        - a string: name of the property,\n"
            "        - a string: value of the property.\n"
            "    \n";
    addCommand("setProperty", new command::SetProperty(*this, docstring));

    docstring = "    \n"
            "    Get a property\n"
            "    \n"
            "      Input:\n"
            "        - a string: name of the property,\n"
            "      Return:\n"
            "        - a string: value of the property\n";
    addCommand("getProperty", new command::GetProperty(*this, docstring));

    // #### End Work in progress
}

Dynamic::~Dynamic( void )
{
    if (this->m_data) delete this->m_data;
    for(  std::list< SignalBase<int>* >::iterator iter = genericSignalRefs.begin();
          iter != genericSignalRefs.end();
          ++iter )
    {
        SignalBase<int>* sigPtr = *iter;
        delete sigPtr;
    }
    return;
}

/* --- CONFIG --------------------------------------------------------------- */
/* --- CONFIG --------------------------------------------------------------- */
/* --- CONFIG --------------------------------------------------------------- */

void Dynamic::setUrdfPath( const std::string& path )
{
    this->m_model = se3::urdf::buildModel(path, true);
    this->m_urdfPath = path;
    if (this->m_data) delete this->m_data;
    this->m_data = new se3::Data(m_model);
    init=true;
}

/* --- CONVERTION ---------------------------------------------------- */
/* --- CONVERTION ---------------------------------------------------- */
/* --- CONVERTION ---------------------------------------------------- */

static Eigen::VectorXd maalToEigenVectorXd(const ml::Vector& inVector)
{
    Eigen::VectorXd vector(inVector.size());
    for (unsigned int r=0; r<inVector.size(); r++)
        vector(r) = inVector(r);
    return vector;
}

static Eigen::MatrixXd maalToEigenMatrixXd(const ml::Matrix& inMatrix)
{
    Eigen::MatrixXd matrix(inMatrix.nbRows(),inMatrix.nbCols());
    for (unsigned int r=0; r<inMatrix.nbRows(); r++)
        for (unsigned int c=0; c<inMatrix.nbCols(); c++)
            matrix(r,c) = inMatrix(r,c);
    return matrix;
}

static ml::Vector eigenVectorXdToMaal(const Eigen::VectorXd& inVector)
{
    ml::Vector vector(inVector.size());
    for (unsigned int r=0; r<inVector.size(); r++)
        vector(r) = inVector(r);
    return vector;
}

static ml::Matrix eigenMatrixXdToMaal(const Eigen::MatrixXd& inMatrix)
{
    ml::Matrix matrix(inMatrix.rows(),inMatrix.cols());
    for (unsigned int r=0; r<inMatrix.rows(); r++)
        for (unsigned int c=0; c<inMatrix.cols(); c++)
            matrix(r,c) = inMatrix(r,c);
    return matrix;
}

static void eigenVector3dToMaal( const Eigen::Vector3d& source,
                                 ml::Vector & res )
{
    sotDEBUG(5) << source <<endl;
    res(0) = source[0];
    res(1) = source[1];
    res(2) = source[2];
}

Eigen::VectorXd Dynamic::getPinocchioPos(int time)
{
    const Eigen::VectorXd qJoints=maalToEigenVectorXd(jointPositionSIN.access(time));
    const Eigen::VectorXd qFF=maalToEigenVectorXd(freeFlyerPositionSIN.access(time));
    Eigen::VectorXd q(qJoints.size() + 7);// assert qFF.size() = 6?
    urdf::Rotation rot;
    rot.setFromRPY(qFF(3),qFF(4),qFF(5));
    double x,y,z,w;
    double &refx = x;
    double &refy = y;
    double &refz = z;
    double &refw = w;
    rot.getQuaternion(refx,refy,refz,refw);

    q << qFF(0),qFF(1),qFF(2),x,y,z,w,qJoints;// assert q.size()==m_model.nq?
    return q;
}

Eigen::VectorXd Dynamic::getPinocchioVel(int time)
{
    const Eigen::VectorXd vJoints=maalToEigenVectorXd(jointVelocitySIN.access(time));
    const Eigen::VectorXd vFF=maalToEigenVectorXd(freeFlyerVelocitySIN.access(time));
    Eigen::VectorXd v(vJoints.size() + vFF.size());

    v << vFF,vJoints;// assert q.size()==m_model.nq?
    return v;
}

Eigen::VectorXd Dynamic::getPinocchioAcc(int time)
{
    const Eigen::VectorXd aJoints=maalToEigenVectorXd(jointAccelerationSIN.access(time));
    const Eigen::VectorXd aFF=maalToEigenVectorXd(freeFlyerAccelerationSIN.access(time));
    Eigen::VectorXd a(aJoints.size() + aFF.size());

    a << aFF,aJoints;// assert q.size()==m_model.nq?
    return a;
}

/* --- SIGNAL ACTIVATION ---------------------------------------------------- */
/* --- SIGNAL ACTIVATION ---------------------------------------------------- */
/* --- SIGNAL ACTIVATION ---------------------------------------------------- */

dg::SignalTimeDependent< ml::Matrix,int > & Dynamic::
createEndeffJacobianSignal( const std::string& signame, int jointId )
{
    //Work done
    dg::SignalTimeDependent< ml::Matrix,int > * sig
            = new dg::SignalTimeDependent< ml::Matrix,int >
            ( boost::bind(&Dynamic::computeGenericJacobian,this,jointId,_1,_2),
              newtonEulerSINTERN,
              "sotDynamic("+name+")::output(matrix)::"+signame );

    genericSignalRefs.push_back( sig );
    signalRegistration( *sig );
    return *sig;
}

dg::SignalTimeDependent< ml::Matrix,int > & Dynamic::
createJacobianSignal( const std::string& signame, int jointId )
{
    //Work in progress
    dg::SignalTimeDependent< ml::Matrix,int > * sig
            = new dg::SignalTimeDependent< ml::Matrix,int >
            ( boost::bind(&Dynamic::computeGenericJacobian,this,jointId,_1,_2),
              newtonEulerSINTERN,
              "sotDynamic("+name+")::output(matrix)::"+signame );

    genericSignalRefs.push_back( sig );
    signalRegistration( *sig );
    return *sig;
}

void Dynamic::
destroyJacobianSignal( const std::string& signame )
{
    //Work done
    bool deletable = false;
    dg::SignalTimeDependent< ml::Matrix,int > * sig = & jacobiansSOUT( signame );
    for(  std::list< SignalBase<int>* >::iterator iter = genericSignalRefs.begin();
          iter != genericSignalRefs.end();
          ++iter )
    {
        if( (*iter) == sig ) { genericSignalRefs.erase(iter); deletable = true; break; }
    }
    if(! deletable )
    {
        SOT_THROW ExceptionDynamic( ExceptionDynamic::CANT_DESTROY_SIGNAL,
                                    "Cannot destroy signal",
                                    " (while trying to remove generic jac. signal <%s>).",
                                    signame.c_str() );
    }
    signalDeregistration( signame );

    delete sig;
}

/* --- POINT --- */
/* --- POINT --- */
/* --- POINT --- */

dg::SignalTimeDependent< MatrixHomogeneous,int >& Dynamic::
createPositionSignal ( const std::string& signame, int jointId )
{
    //Work done
    sotDEBUGIN(15);

    dg::SignalTimeDependent< MatrixHomogeneous,int > * sig
            = new dg::SignalTimeDependent< MatrixHomogeneous,int >
            ( boost::bind(&Dynamic::computeGenericPosition,this,jointId,_1,_2),
              newtonEulerSINTERN,
              "sotDynamic("+name+")::output(matrixHomo)::"+signame );

    genericSignalRefs.push_back( sig );
    signalRegistration( *sig );

    sotDEBUGOUT(15);
    return *sig;;
}

void Dynamic::
destroyPositionSignal( const std::string& signame )
{
    //work done
    bool deletable = false;
    dg::SignalTimeDependent< MatrixHomogeneous,int > * sig = & positionsSOUT( signame );
    for(  std::list< SignalBase<int>* >::iterator iter = genericSignalRefs.begin();
          iter != genericSignalRefs.end();
          ++iter )
    {
        if( (*iter) == sig ) { genericSignalRefs.erase(iter); deletable = true; break; }
    }
    if(! deletable )
    {
        SOT_THROW ExceptionDynamic( ExceptionDynamic::CANT_DESTROY_SIGNAL,
                                    "Cannot destroy signal",
                                    " (while trying to remove generic pos. signal <%s>).",
                                    signame.c_str() );
    }
    signalDeregistration( signame );

    delete sig;
}

/* --- VELOCITY --- */
/* --- VELOCITY --- */
/* --- VELOCITY --- */

dg::SignalTimeDependent< ml::Vector,int >& Dynamic::
createVelocitySignal( const std::string& signame,  int jointId )
{
    //Work in progress
    sotDEBUGIN(15);
    SignalTimeDependent< ml::Vector,int > * sig
            = new SignalTimeDependent< ml::Vector,int >
            ( boost::bind(&Dynamic::computeGenericVelocity,this,jointId,_1,_2),
              newtonEulerSINTERN,
              "sotDynamic("+name+")::output(ml::Vector)::"+signame );
    genericSignalRefs.push_back( sig );
    signalRegistration( *sig );

    sotDEBUGOUT(15);
    return *sig;
}

void Dynamic::
destroyVelocitySignal( const std::string& signame )
{
    //Work in progress
    bool deletable = false;
    SignalTimeDependent< ml::Vector,int > * sig = & velocitiesSOUT( signame );
    for(  std::list< SignalBase<int>* >::iterator iter = genericSignalRefs.begin();
          iter != genericSignalRefs.end();
          ++iter )
    {
        if( (*iter) == sig ) { genericSignalRefs.erase(iter); deletable = true; break; }
    }
    if(! deletable )
    {
        SOT_THROW ExceptionDynamic( ExceptionDynamic::CANT_DESTROY_SIGNAL,
                                    "Cannot destroy signal",
                                    " (while trying to remove generic pos. signal <%s>).",
                                    signame.c_str() );
    }
    signalDeregistration( signame );
    delete sig;
}

/* --- ACCELERATION --- */
/* --- ACCELERATION --- */
/* --- ACCELERATION --- */

dg::SignalTimeDependent< ml::Vector,int >& Dynamic::
createAccelerationSignal( const std::string& signame, int jointId )
{
    //Work in progress
    sotDEBUGIN(15);
    dg::SignalTimeDependent< ml::Vector,int > * sig
            = new dg::SignalTimeDependent< ml::Vector,int >
            ( boost::bind(&Dynamic::computeGenericAcceleration,this,jointId,_1,_2),
              newtonEulerSINTERN,
              "sotDynamic("+name+")::output(matrixHomo)::"+signame );

    genericSignalRefs.push_back( sig );
    signalRegistration( *sig );

    sotDEBUGOUT(15);
    return *sig;
}

void Dynamic::
destroyAccelerationSignal( const std::string& signame )
{
    //Work in progress
    bool deletable = false;
    dg::SignalTimeDependent< ml::Vector,int > * sig = & accelerationsSOUT( signame );
    for(  std::list< SignalBase<int>* >::iterator iter = genericSignalRefs.begin();
          iter != genericSignalRefs.end();
          ++iter )
    {
        if( (*iter) == sig ) { genericSignalRefs.erase(iter); deletable = true; break; }
    }
    if(! deletable )
    {
        SOT_THROW ExceptionDynamic( ExceptionDynamic::CANT_DESTROY_SIGNAL,
                                    getName() + ":cannot destroy signal",
                                    " (while trying to remove generic acc "
                                    "signal <%s>).",
                                    signame.c_str() );
    }
    signalDeregistration( signame );
    delete sig;
}



/* --- COMPUTE -------------------------------------------------------------- */
/* --- COMPUTE -------------------------------------------------------------- */
/* --- COMPUTE -------------------------------------------------------------- */


ml::Matrix& Dynamic::computeGenericJacobian( int jointId,ml::Matrix& res,int time )
{
    // Useless
    // Work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);

   res = eigenMatrixXdToMaal( se3::computeJacobians(this->m_model,*this->m_data,this->getPinocchioPos(time)));

    sotDEBUGOUT(25);

    return res;
}

ml::Matrix& Dynamic::computeGenericEndeffJacobian( int jointId,ml::Matrix& res,int time )
{
    //Work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);

    Eigen::MatrixXd J (Eigen::MatrixXd::Zero(6,this->m_model.nv));
    se3::getJacobian<false>(this->m_model, *this->m_data,(se3::Model::Index)jointId,J);
    res = eigenMatrixXdToMaal(J);

    sotTDEBUGOUT(25);
    return res;
}

MatrixHomogeneous& Dynamic::computeGenericPosition( int jointId,MatrixHomogeneous& res,int time )
{
    //Work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);

    se3::SE3 se3tmp = this->m_data->oMi[jointId];
    res.initFromMotherLib(eigenMatrixXdToMaal(se3tmp.toHomogeneousMatrix()).accessToMotherLib());

    sotTDEBUGOUT(25);
    return res;
}

ml::Vector& Dynamic::computeGenericVelocity( int j,ml::Vector& res,int time )
{
    //work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);

    se3::Motion aRV = this->m_data->v[j];
    se3::MotionTpl<double>::Vector3 al= aRV.linear();
    se3::MotionTpl<double>::Vector3 ar= aRV.angular();

    res.resize(6);
    for( int i=0;i<3;++i )
    {
        res(i)=al(i);
        res(i+3)=ar(i);
    }
    sotDEBUGOUT(25);
    return res;
}

ml::Vector& Dynamic::computeGenericAcceleration( int j,ml::Vector& res,int time )
{
    //work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    se3::Motion aRA = this->m_data->a[j];
    se3::MotionTpl<double>::Vector3 al= aRA.linear();
    se3::MotionTpl<double>::Vector3 ar= aRA.angular();

    res.resize(6);
    for( int i=0;i<3;++i )
    {
        res(i)=al(i);
        res(i+3)=ar(i);
    }

    sotDEBUGOUT(25);
    return res;
}

ml::Vector& Dynamic::computeZmp( ml::Vector& res,int time )
{
    //To be verified
    sotDEBUGIN(25);
    if (res.size()!=3)
        res.resize(3);

    newtonEulerSINTERN(time);
    se3::Force ftau = this->m_data->f[0];  //com ?
    //ml::Vector tau(3);
    se3::Force::Vector3 tau = ftau.angular();
    se3::Force::Vector3 f = ftau.linear();
    res(2) = 0;
    res(0) = -tau[1]/f[2];//tau : x y z ?
    res(1) = tau[0]/f[2];

    sotDEBUGOUT(25);

    return res;
}

ml::Vector& Dynamic::computeMomenta( ml::Vector &res, int time)
{
    //Work in progress
    se3::Force::Vector3 linearMomentum, angularMomentum;
    if(res.size()!=6) res.resize(6);

    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    se3::Force ftau = this->m_data->f[0];
    linearMomentum = ftau.linear();
    angularMomentum = ftau.angular();

    for(unsigned int i=3; i<3;i++ )
    {
        res(i) = linearMomentum(i);
        res(i+3) = angularMomentum(i);
    }

    sotDEBUGOUT(25) << "Momenta : " << res;
    return res;
}

ml::Vector& Dynamic::computeAngularMomentum( ml::Vector &res, int time)
{
    // work in progress
    se3::Force::Vector3 angularMomentum;
    if(res.size()!=3) res.resize(3);

    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    se3::Force ftau = this->m_data->f[0];
    angularMomentum = ftau.angular();

    for(unsigned int i=3; i<3;i++ ) res(i+3) = angularMomentum(i);

    sotDEBUGOUT(25) << "AngularMomenta : " << res;
    return res;
}

ml::Matrix& Dynamic::computeJcom( ml::Matrix& res,int time )
{
    //Work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);

    res.initFromMotherLib(eigenMatrixXdToMaal(m_data->Jcom).accessToMotherLib());
    sotDEBUGOUT(25);
    return res;
}

ml::Vector& Dynamic::computeCom( ml::Vector& res,int time )
{
    //Work in progress
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    res.resize(3);
    eigenVector3dToMaal(this->m_data->com[0],res);
    sotDEBUGOUT(25);
    return res;
}

ml::Matrix& Dynamic::computeInertia( ml::Matrix& res,int time )
{
    //work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    res=eigenMatrixXdToMaal(se3::crba(this->m_model,*this->m_data,this->getPinocchioPos(time)));
    sotDEBUGOUT(25);
    return res;
}

ml::Matrix& Dynamic::computeInertiaReal( ml::Matrix& res,int time )
{
    //Work in progress
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    const ml::Matrix & A = inertiaSOUT(time);
    const ml::Vector & gearRatio = gearRatioSOUT(time);
    const ml::Vector & inertiaRotor = inertiaRotorSOUT(time);

    res = A;
    for( unsigned int i=0;i<gearRatio.size();++i )
        res(i,i) += (gearRatio(i)*gearRatio(i)*inertiaRotor(i));

    sotDEBUGOUT(25);
    return res;
}

double& Dynamic::computeFootHeight( double& res,int time )
{
    //Work in progress
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    if(!this->m_model.existBodyName("rigthFoot"))
    {
        throw runtime_error ("Robot has no joint corresponding to rigthFoot");
    }
    int jointId = this->m_model.getBodyId("rigthFoot");

    se3::SE3 joint = this->m_data->oMi[jointId];
    MatrixHomogeneous tmpMat;
    tmpMat.initFromMotherLib(eigenMatrixXdToMaal(joint.toHomogeneousMatrix()).accessToMotherLib());
    res = tmpMat(3,2);
    sotDEBUGOUT(25);
    return res;
}

ml::Vector& Dynamic::computeTorqueDrift( ml::Vector& res,const int& time )
{
    //Work done
    sotDEBUGIN(25);
    newtonEulerSINTERN(time);
    const unsigned int NB_JOINTS = jointPositionSIN.accessCopy().size();

    res.resize(NB_JOINTS);
    res = eigenVectorXdToMaal(this->m_data->tau);

    sotDEBUGOUT(25);
    return res;
}

/* --- SIGNAL --------------------------------------------------------------- */
/* --- SIGNAL --------------------------------------------------------------- */
/* --- SIGNAL --------------------------------------------------------------- */
dg::SignalTimeDependent<ml::Matrix,int>& Dynamic::jacobiansSOUT( const std::string& name )
{
    //Work in progress
    SignalBase<int> & sigabs = Entity::getSignal(name);

    try {
        dg::SignalTimeDependent<ml::Matrix,int>& res
                = dynamic_cast< dg::SignalTimeDependent<ml::Matrix,int>& >( sigabs );
        return res;
    } catch( std::bad_cast e ) {
        SOT_THROW ExceptionSignal( ExceptionSignal::BAD_CAST,
                                   "Impossible cast.",
                                   " (while getting signal <%s> of type matrix.",
                                   name.c_str());
    }
}

dg::SignalTimeDependent<MatrixHomogeneous,int>& Dynamic::positionsSOUT( const std::string& name )
{
    //Work in progress
    SignalBase<int> & sigabs = Entity::getSignal(name);

    try {
        dg::SignalTimeDependent<MatrixHomogeneous,int>& res
                = dynamic_cast< dg::SignalTimeDependent<MatrixHomogeneous,int>& >( sigabs );
        return res;
    } catch( std::bad_cast e ) {
        SOT_THROW ExceptionSignal( ExceptionSignal::BAD_CAST,
                                   "Impossible cast.",
                                   " (while getting signal <%s> of type matrixHomo.",
                                   name.c_str());
    }
}

dg::SignalTimeDependent<ml::Vector,int>& Dynamic::velocitiesSOUT( const std::string& name )
{
    //Work in progress
    SignalBase<int> & sigabs = Entity::getSignal(name);
    try {
        dg::SignalTimeDependent<ml::Vector,int>& res
                = dynamic_cast< dg::SignalTimeDependent<ml::Vector,int>& >( sigabs );
        return res;
    } catch( std::bad_cast e ) {
        SOT_THROW ExceptionSignal( ExceptionSignal::BAD_CAST,
                                   "Impossible cast.",
                                   " (while getting signal <%s> of type Vector.",
                                   name.c_str());
    }
}

dg::SignalTimeDependent<ml::Vector,int>& Dynamic::accelerationsSOUT( const std::string& name )
{
    //Work in progress
    SignalBase<int> & sigabs = Entity::getSignal(name);

    try {
        dg::SignalTimeDependent<ml::Vector,int>& res
                = dynamic_cast< dg::SignalTimeDependent<ml::Vector,int>& >( sigabs );
        return res;
    } catch( std::bad_cast e ) {
        SOT_THROW ExceptionSignal( ExceptionSignal::BAD_CAST,
                                   "Impossible cast.",
                                   " (while getting signal <%s> of type Vector.",
                                   name.c_str());
    }
}

int& Dynamic::computeNewtonEuler( int& dummy,int time )
{
    const Eigen::VectorXd q=getPinocchioPos(time);
    const Eigen::VectorXd v=getPinocchioVel(time);
    const Eigen::VectorXd a=getPinocchioAcc(time);
    se3::rnea(m_model,*m_data,q,v,a);
    //se3::kinematics(m_model,*m_data,q,v);

    return dummy;
}

ml::Vector& Dynamic::getUpperJointLimits( ml::Vector& res,const int& time )
{
    //Work done
    sotDEBUGIN(15);
    newtonEulerSINTERN(time);
    res = eigenVectorXdToMaal(this->m_data->upperPositionLimit);

    sotDEBUGOUT(15);
    return res;
}

ml::Vector& Dynamic::getLowerJointLimits( ml::Vector& res,const int& time )
{
    //Work done
    sotDEBUGIN(15);
    newtonEulerSINTERN(time);
    res = eigenVectorXdToMaal(this->m_data->lowerPositionLimit);

    sotDEBUGOUT(15);
    return res;
}

ml::Vector& Dynamic::getUpperVelocityLimits( ml::Vector& res,const int& time )
{
    //Work done
    sotDEBUGIN(15);
    newtonEulerSINTERN(time);
    res = eigenVectorXdToMaal(this->m_data->velocityLimit);

    sotDEBUGOUT(15);
    return res;
}

ml::Vector& Dynamic::getUpperTorqueLimits( ml::Vector& res,const int& time )
{
    //Work done
    sotDEBUGIN(15);
    newtonEulerSINTERN(time);
    res = eigenVectorXdToMaal(this->m_data->effortLimit);

    sotDEBUGOUT(15);
    return res;
}


/* --- COMMANDS ------------------------------------------------------------- */
/* --- COMMANDS ------------------------------------------------------------- */
/* --- COMMANDS ------------------------------------------------------------- */

void Dynamic::cmd_createOpPointSignals( const std::string& opPointName,
                                        const std::string& jointName )
{
    if(!this->m_model.existBodyName(jointName))
    {
        throw runtime_error ("Robot has no joint corresponding to " + jointName);
    }
    int jointId = this->m_model.getBodyId(jointName);
    createEndeffJacobianSignal(std::string("J")+opPointName,jointId);
    createPositionSignal(opPointName,jointId);
}

void Dynamic::cmd_createJacobianWorldSignal( const std::string& signalName,
                                             const std::string& jointName )
{
    //Work in progress
    if(!this->m_model.existBodyName(jointName))
    {
        throw runtime_error ("Robot has no joint corresponding to " + jointName);
    }
    int jointId = this->m_model.getBodyId(jointName);
    createJacobianSignal(signalName, jointId);
}

void Dynamic::cmd_createJacobianEndEffectorSignal( const std::string& signalName,
                                                   const std::string& jointName )
{
    //Work in progress
    if(!this->m_model.existBodyName(jointName))
    {
        throw runtime_error ("Robot has no joint corresponding to " + jointName);
    }
    int jointId = this->m_model.getBodyId(jointName);
    createEndeffJacobianSignal(signalName, jointId);
}

void Dynamic::cmd_createPositionSignal( const std::string& signalName,
                                        const std::string& jointName )
{
    //Work in progress
    if(!this->m_model.existBodyName(jointName))
    {
        throw runtime_error ("Robot has no joint corresponding to " + jointName);
    }
    int jointId = this->m_model.getBodyId(jointName);
    createPositionSignal(signalName, jointId);
}

/* --- PARAMS --------------------------------------------------------------- */
/* --- PARAMS --------------------------------------------------------------- */
/* --- PARAMS --------------------------------------------------------------- */

void Dynamic::commandLine( const std::string& cmdLine,
                           std::istringstream& cmdArgs,
                           std::ostream& os )
{
    sotDEBUGIN(25) << "# In { Cmd " << cmdLine <<endl;
    string filename;
    string Jname;
    unsigned int rank;
    unsigned int b;

    switch(mapCmdLine[cmdLine]){
    case debugInertia:
        cmdArgs>>ws;
        if(cmdArgs.good())
        {
            std::string arg; cmdArgs >> arg;
            if( (arg=="true")||(arg=="1") )
            { debuginertia = 1; }
            else if( (arg=="2")||(arg=="grip") )
            { debuginertia = 2; }
            else debuginertia=0;

        }
        else { os << "debugInertia = " << debuginertia << std::endl; }
        break;
    case setFiles :
        cmdArgs>>filename;
        setUrdfPath(filename);
        break;
    case displayFiles:
    {
        cmdArgs >> ws;
        std::string filetype; cmdArgs >> filetype;
        sotDEBUG(15) << " Request: " << filetype << std::endl;
        os << m_urdfPath << std::endl;
        break;
    }
    case parse:
        if(! init) cout << "No file parsed, run command setFiles" << endl;
        else cout << "Already parsed!" << endl;
        break;
    case createJacobian:
        cmdArgs >> Jname;
        cmdArgs >> rank;
        createJacobianSignal(Jname,rank);
        break;
    case destroyJacobian:
        cmdArgs >> Jname;
        destroyJacobianSignal(Jname);
        break;
    case createPosition:
        cmdArgs >> Jname;
        cmdArgs >> rank;
        createPositionSignal(Jname,rank);
        break;
    case destroyPosition:
        cmdArgs >> Jname;
        destroyPositionSignal(Jname);
        break;
    case createVelocity:
        cmdArgs >> Jname;
        cmdArgs >> rank;
        createVelocitySignal(Jname,rank);
        break;
    case destroyVelocity:
        cmdArgs >> Jname;
        destroyVelocitySignal(Jname);
        break;
    case createAcceleration:
        cmdArgs >> Jname;
        cmdArgs >> rank;
        createAccelerationSignal(Jname,rank);
        break;
    case destroyAcceleration:
        cmdArgs >> Jname;
        destroyAccelerationSignal(Jname);
        break;
    case createEndeffJacobian:
        cmdArgs >> Jname;
        cmdArgs >> rank;
        createEndeffJacobianSignal(Jname,rank);
        break;
    case createOpPoint:
        cmdArgs >> Jname;
        cmdArgs >> rank;
        createEndeffJacobianSignal(string("J")+Jname,rank);
        createPositionSignal(Jname,rank);
        sotDEBUG(15)<<endl;
        break;
    case destroyOpPoint:
        cmdArgs >> Jname;
        destroyJacobianSignal(string("J")+Jname);
        destroyPositionSignal(Jname);
        break;
    case ndof:
        break;
    case setComputeCom:
        // useless
        cmdArgs >> b;
        comActivation((b!=0));
        break;
    case setComputeZmp:
        // useless
        cmdArgs >> b;
        zmpActivation((b!=0));
        break;
    case setProperty:
        // useless
        break;
    case getProperty:
        // useless
        break;
    case displayProperties:
        // useless
        break;
    case help:
        break;
    default:
        break;
    }

    sotDEBUGOUT(25);
}

void Dynamic::createRobot()
{
    if (this->m_model.nq!=0)
    {
        this->m_model.~Model();
        this->m_model=se3::Model();
        delete this->m_data;
        this->m_data = NULL;
    }
}

void Dynamic::createJoint(const std::string& inJointName,
              const std::string& inJointType,
               const ml::Matrix& inPosition)
{
    /*
  if (jointMap_.count(inJointName) == 1) {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   "a joint with name " + inJointName +
                   " has already been created.");
  }
  matrix4d position = maalToMatrix4d(inPosition);
  CjrlJoint* joint=NULL;

  if (inJointType == "freeflyer") {
    joint = factory_.createJointFreeflyer(position);
  } else if (inJointType == "rotation") {
    joint = factory_.createJointRotation(position);
  } else if (inJointType == "translation") {
    joint = factory_.createJointTranslation(position);
  } else if (inJointType == "anchor") {
    joint = factory_.createJointAnchor(position);
  } else {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   inJointType + " is not a valid type.\n"
                   "Valid types are 'freeflyer', 'rotation', 'translation', 'anchor'.");
  }
  jointMap_[inJointName] = joint;
  */
}

void Dynamic::setRootJoint(const std::string& inJointName)
{
    /*
  if (!m_HDR) {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   "you must create a robot first.");
  }
  if (jointMap_.count(inJointName) != 1) {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   "No joint with name " + inJointName +
                   " has been created.");
  }
  m_HDR->rootJoint(*jointMap_[inJointName]);
  */
}

void Dynamic::addJoint(const std::string& inParentName,
               const std::string& inChildName)
{
    /*
  if (!m_HDR) {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   "you must create a robot first.");
  }
  if (jointMap_.count(inParentName) != 1) {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   "No joint with name " + inParentName +
                   " has been created.");
  }
  if (jointMap_.count(inChildName) != 1) {
    SOT_THROW ExceptionDynamic(ExceptionDynamic::DYNAMIC_JRL,
                   "No joint with name " + inChildName +
                   " has been created.");
  }
  jointMap_[inParentName]->addChildJoint(*(jointMap_[inChildName]));
  */
}
