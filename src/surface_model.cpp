#include "surfaceModel.h"

    /* constructor */
    reflection::reflection(Particles* _particles, double _dt,
#if __CUDACC__
                            curandState *_state,
#else
                            std::mt19937 *_state,
#endif
            int _nLines,Boundary * _boundaryVector,
            Surfaces * _surfaces,
    int _nE_sputtRefCoeff,
    int _nA_sputtRefCoeff,
    gitr_precision* _A_sputtRefCoeff,
    gitr_precision* _Elog_sputtRefCoeff,
    gitr_precision* _spyl_surfaceModel,
    gitr_precision* _rfyl_surfaceModel,
    int _nE_sputtRefDistOut,
    int _nE_sputtRefDistOutRef,
    int _nA_sputtRefDistOut,
    int _nE_sputtRefDistIn,
    int _nA_sputtRefDistIn,
    gitr_precision* _E_sputtRefDistIn,
    gitr_precision* _A_sputtRefDistIn,
    gitr_precision* _E_sputtRefDistOut,
    gitr_precision* _E_sputtRefDistOutRef,
    gitr_precision* _A_sputtRefDistOut,
    gitr_precision* _energyDistGrid01,
    gitr_precision* _energyDistGrid01Ref,
    gitr_precision* _angleDistGrid01,
    gitr_precision* _EDist_CDF_Y_regrid,
    gitr_precision* _ADist_CDF_Y_regrid, 
    gitr_precision* _EDist_CDF_R_regrid,
    gitr_precision* _ADist_CDF_R_regrid,
    int _nEdist,
    gitr_precision _E0dist,
    gitr_precision _Edist,
    int _nAdist,
    gitr_precision _A0dist,
    gitr_precision _Adist) :
particles(_particles),
                             dt(_dt),
                             nLines(_nLines),
                             boundaryVector(_boundaryVector),
                             surfaces(_surfaces),
                             nE_sputtRefCoeff(_nE_sputtRefCoeff),
                             nA_sputtRefCoeff(_nA_sputtRefCoeff),
                             A_sputtRefCoeff(_A_sputtRefCoeff),
                             Elog_sputtRefCoeff(_Elog_sputtRefCoeff),
                             spyl_surfaceModel(_spyl_surfaceModel),
                             rfyl_surfaceModel(_rfyl_surfaceModel),
                             nE_sputtRefDistOut(_nE_sputtRefDistOut),
                             nE_sputtRefDistOutRef(_nE_sputtRefDistOutRef),
                             nA_sputtRefDistOut(_nA_sputtRefDistOut),
                             nE_sputtRefDistIn(_nE_sputtRefDistIn),
                             nA_sputtRefDistIn(_nA_sputtRefDistIn),
                             E_sputtRefDistIn(_E_sputtRefDistIn),
                             A_sputtRefDistIn(_A_sputtRefDistIn),
                             E_sputtRefDistOut(_E_sputtRefDistOut),
                             E_sputtRefDistOutRef(_E_sputtRefDistOutRef),
                             A_sputtRefDistOut(_A_sputtRefDistOut),
                             energyDistGrid01(_energyDistGrid01),
                             energyDistGrid01Ref(_energyDistGrid01Ref),
                             angleDistGrid01(_angleDistGrid01),
                             EDist_CDF_Y_regrid(_EDist_CDF_Y_regrid),
                             ADist_CDF_Y_regrid(_ADist_CDF_Y_regrid),
                             EDist_CDF_R_regrid(_EDist_CDF_R_regrid),
                             ADist_CDF_R_regrid(_ADist_CDF_R_regrid),
                             nEdist(_nEdist),
                             E0dist(_E0dist),
                             Edist(_Edist),
                             nAdist(_nAdist),
                             A0dist(_A0dist),
                             Adist(_Adist),
                             state(_state) { }

CUDA_CALLABLE_MEMBER_DEVICE
void reflection::operator()(std::size_t indx) const {
    
    if (particles->hitWall[indx] == 1.0) {
      gitr_precision E0 = 0.0;
      gitr_precision thetaImpact = 0.0;
      gitr_precision particleTrackVector[3] = {0.0f};
      gitr_precision surfaceNormalVector[3] = {0.0f};
      gitr_precision vSampled[3] = {0.0f};
      gitr_precision norm_part = 0.0;
      int signPartDotNormal = 0;
      gitr_precision partDotNormal = 0.0;
      gitr_precision Enew = 0.0f;
      gitr_precision angleSample = 0.0f;
      int wallIndex = 0;
      gitr_precision tol = 1e12;
      gitr_precision Sr = 0.0;
      gitr_precision St = 0.0;
      gitr_precision Y0 = 0.0;
      gitr_precision R0 = 0.0;
      gitr_precision totalYR = 0.0;
      gitr_precision newWeight = 0.0;
      int wallHit = particles->surfaceHit[indx];
      int surfaceHit = boundaryVector[wallHit].surfaceNumber;
      int surface = boundaryVector[wallHit].surface;
      ////FIXME
      //if (wallHit > 260)
      //  wallHit = 260;
      //if (wallHit < 0)
      //  wallHit = 0;
      //if (surfaceHit > 260)
      //  surfaceHit = 260;
      //if (surfaceHit < 0)
      //  surfaceHit = 0;
      //if (surface > 260)
      //  surface = 260;
      //if (surface < 0)
      //  surface = 0;
      gitr_precision eInterpVal = 0.0;
      gitr_precision aInterpVal = 0.0;
      gitr_precision weight = particles->weight[indx];
      gitr_precision vx = particles->vx[indx];
      gitr_precision vy = particles->vy[indx];
      gitr_precision vz = particles->vz[indx];
#if FLUX_EA > 0
      gitr_precision dEdist = (Edist - E0dist) / static_cast<gitr_precision>(nEdist);
      gitr_precision dAdist = (Adist - A0dist) / static_cast<gitr_precision>(nAdist);
      int AdistInd = 0;
      int EdistInd = 0;
#endif
      particles->firstCollision[indx] = 1;
      particleTrackVector[0] = vx;
      particleTrackVector[1] = vy;
      particleTrackVector[2] = vz;
      norm_part = std::sqrt(particleTrackVector[0] * particleTrackVector[0] + particleTrackVector[1] * particleTrackVector[1] + particleTrackVector[2] * particleTrackVector[2]);
      E0 = 0.5 * particles->amu[indx] * 1.6737236e-27 * (norm_part * norm_part) / 1.60217662e-19;
      if (E0 > 1000.0)
        E0 = 990.0;
      //std::cout << "Particle hit wall with energy " << E0 << std::endl;
      //std::cout << "Particle hit wall with v " << vx << " " << vy << " " << vz<< std::endl;
      //std::cout << "Particle amu norm_part " << particles->amu[indx] << " " << vy << " " << vz<< std::endl;
      wallIndex = particles->wallIndex[indx];
      boundaryVector[wallHit].getSurfaceNormal(surfaceNormalVector, particles->y[indx], particles->x[indx]);
      particleTrackVector[0] = particleTrackVector[0] / norm_part;
      particleTrackVector[1] = particleTrackVector[1] / norm_part;
      particleTrackVector[2] = particleTrackVector[2] / norm_part;

      partDotNormal = vectorDotProduct(particleTrackVector, surfaceNormalVector);
      thetaImpact = std::acos(partDotNormal);
      if (thetaImpact > 3.14159265359 * 0.5) {
        thetaImpact = std::abs(thetaImpact - (3.14159265359));
      }
      thetaImpact = thetaImpact * 180.0 / 3.14159265359;
      if (thetaImpact < 0.0)
        thetaImpact = 0.0;
      signPartDotNormal = std::copysign(1.0,partDotNormal);
      if (E0 == 0.0) {
        thetaImpact = 0.0;
      }
      if (boundaryVector[wallHit].Z > 0.0) {
        Y0 = interp2d(thetaImpact, std::log10(E0), nA_sputtRefCoeff,
                      nE_sputtRefCoeff, A_sputtRefCoeff,
                      Elog_sputtRefCoeff, spyl_surfaceModel);
        R0 = interp2d(thetaImpact, std::log10(E0), nA_sputtRefCoeff,
                      nE_sputtRefCoeff, A_sputtRefCoeff,
                      Elog_sputtRefCoeff, rfyl_surfaceModel);
      } else {
        Y0 = 0.0;
        R0 = 0.0;
      }
      //std::cout << "Particle " << indx << " struck surface with energy and angle " << E0 << " " << thetaImpact << std::endl;
      //std::cout << " resulting in Y0 and R0 of " << Y0 << " " << R0 << std::endl;
      totalYR = Y0 + R0;
//if(particles->test[indx] == 0.0)
//{
//    particles->test[indx] = 1.0;
//    particles->test0[indx] = E0;
//    particles->test1[indx] = thetaImpact;
//    particles->test2[indx] = Y0;
//    particles->test3[indx] = R0;
//}
//std::cout << "Energy angle yield " << E0 << " " << thetaImpact << " " << Y0 << std::endl;
#if PARTICLESEEDS > 0
#ifdef __CUDACC__
      gitr_precision r7 = curand_uniform(&state[indx]);
      gitr_precision r8 = curand_uniform(&state[indx]);
      gitr_precision r9 = curand_uniform(&state[indx]);
      gitr_precision r10 = curand_uniform(&state[indx]);
#else
      std::uniform_real_distribution<double> dist(0.0, 1.0);
      gitr_precision r7 = dist(state[indx]);
      gitr_precision r8 = dist(state[indx]);
      gitr_precision r9 = dist(state[indx]);
      gitr_precision r10 = dist(state[indx]);
#endif

            #else
              #if __CUDACC__
                gitr_precision r7 = curand_uniform(&state[6]);
                gitr_precision r8 = curand_uniform(&state[7]);
                gitr_precision r9 = curand_uniform(&state[8]);
              #else
                std::uniform_real_distribution<gitr_precision> dist(0.0, 1.0);
                gitr_precision r7=dist(state[6]);
                gitr_precision r8=dist(state[7]);
                gitr_precision r9=dist(state[8]);
              #endif
                //gitr_precision r7 = 0.0;
            #endif
            //particle either reflects or deposits
            gitr_precision sputtProb = Y0/totalYR;
	    int didReflect = 0;
            //  std::cout << " total YR " << totalYR << " " << Y0 << " " << R0 << std::endl;
            if(totalYR > 0.0)
            {
            if(r7 > sputtProb) //reflects
            {
	          didReflect = 1;
                  aInterpVal = interp3d (r8,thetaImpact,std::log10(E0),
                          nA_sputtRefDistOut,nA_sputtRefDistIn,nE_sputtRefDistIn,
                                    angleDistGrid01,A_sputtRefDistIn,
                                    E_sputtRefDistIn,ADist_CDF_R_regrid);
                   eInterpVal = interp3d ( r9,thetaImpact,std::log10(E0),
                           nE_sputtRefDistOutRef,nA_sputtRefDistIn,nE_sputtRefDistIn,
                                         energyDistGrid01Ref,A_sputtRefDistIn,
                                         E_sputtRefDistIn,EDist_CDF_R_regrid );
                   //newWeight=(R0/(1.0f-sputtProb))*weight;
		   newWeight = weight*(totalYR);
    #if FLUX_EA > 0
              EdistInd = std::floor((eInterpVal-E0dist)/dEdist);
              AdistInd = std::floor((aInterpVal-A0dist)/dAdist);
              if((EdistInd >= 0) && (EdistInd < nEdist) && 
                 (AdistInd >= 0) && (AdistInd < nAdist))
              {
            #if USE_CUDA > 0
                  atomicAdd(&surfaces->reflDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd],newWeight);
            #else      
                  surfaces->reflDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd] = 
                    surfaces->reflDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd] +  newWeight;
            #endif
               }
	       #endif
                  if(surface > 0)
                {

            #if USE_CUDA > 0
                    atomicAdd(&surfaces->grossDeposition[surfaceHit],weight*(1.0-R0));
            #else
                    #pragma omp atomic
                    surfaces->grossDeposition[surfaceHit] = surfaces->grossDeposition[surfaceHit]+weight*(1.0-R0);
            #endif
                }
            }
            else //sputters
            {
                  aInterpVal = interp3d(r8,thetaImpact,std::log10(E0),
                          nA_sputtRefDistOut,nA_sputtRefDistIn,nE_sputtRefDistIn,
                          angleDistGrid01,A_sputtRefDistIn,
                          E_sputtRefDistIn,ADist_CDF_Y_regrid);
                  eInterpVal = interp3d(r9,thetaImpact,std::log10(E0),
                           nE_sputtRefDistOut,nA_sputtRefDistIn,nE_sputtRefDistIn,
                           energyDistGrid01,A_sputtRefDistIn,E_sputtRefDistIn,EDist_CDF_Y_regrid);
            //if(particles->test[indx] == 0.0)
            //{
            //    particles->test[indx] = 1.0;
            //    particles->test0[indx] = aInterpVal;
            //    particles->test1[indx] = eInterpVal;
            //    particles->test2[indx] = r8;
            //    particles->test3[indx] = r9;
            //}            
                //std::cout << " particle sputters with " << eInterpVal << aInterpVal <<  std::endl;
		//printf("particle sputters with E A %f %f \n", eInterpVal, aInterpVal);
                  //newWeight=(Y0/sputtProb)*weight;
		  newWeight=weight*totalYR;
    #if FLUX_EA > 0
              EdistInd = std::floor((eInterpVal-E0dist)/dEdist);
              AdistInd = std::floor((aInterpVal-A0dist)/dAdist);
              if((EdistInd >= 0) && (EdistInd < nEdist) && 
                 (AdistInd >= 0) && (AdistInd < nAdist))
              {
                //std::cout << " particle sputters with " << EdistInd << AdistInd <<  std::endl;
		//printf("particle sputters with E A %i %i \n", EdistInd, AdistInd);
            #if USE_CUDA > 0
                  atomicAdd(&surfaces->sputtDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd],newWeight);
            #else      
                  surfaces->sputtDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd] = 
                    surfaces->sputtDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd] +  newWeight;
              #endif 
              }
	       #endif
                  if(sputtProb == 0.0) newWeight = 0.0;
                   //std::cout << " particle sputtered with newWeight " << newWeight << std::endl;
                  if(surface > 0)
                {

            #if USE_CUDA > 0
                    atomicAdd(&surfaces->grossDeposition[surfaceHit],weight*(1.0-R0));
                    atomicAdd(&surfaces->grossErosion[surfaceHit],newWeight);
                    atomicAdd(&surfaces->aveSputtYld[surfaceHit],Y0);
                    if(weight > 0.0)
                    {
                        atomicAdd(&surfaces->sputtYldCount[surfaceHit],1);
                    }
            #else
                    #pragma omp atomic
                    surfaces->grossDeposition[surfaceHit] = surfaces->grossDeposition[surfaceHit]+weight*(1.0-R0);
                    surfaces->grossErosion[surfaceHit] = surfaces->grossErosion[surfaceHit] + newWeight;
                    surfaces->aveSputtYld[surfaceHit] = surfaces->aveSputtYld[surfaceHit] + Y0;
                    surfaces->sputtYldCount[surfaceHit] = surfaces->sputtYldCount[surfaceHit] + 1;
            #endif
                }
            }
            //std::cout << "eInterpValYR " << eInterpVal << std::endl; 
            }
            else
            {       newWeight = 0.0;
                    particles->hitWall[indx] = 2.0;
                  if(surface > 0)
                {
            #if USE_CUDA > 0
                    atomicAdd(&surfaces->grossDeposition[surfaceHit],weight);
            #else
                    surfaces->grossDeposition[surfaceHit] = surfaces->grossDeposition[surfaceHit]+weight;
            #endif
	        }
            //std::cout << "eInterpValYR_not " << eInterpVal << std::endl; 
            }
            //std::cout << "eInterpVal " << eInterpVal << std::endl; 
	    if(eInterpVal <= 0.0)
            {       newWeight = 0.0;
                    particles->hitWall[indx] = 2.0;
                  if(surface > 0)
                {
		    if(didReflect)
		    {
            #if USE_CUDA > 0
                    atomicAdd(&surfaces->grossDeposition[surfaceHit],weight);
            #else
                    surfaces->grossDeposition[surfaceHit] = surfaces->grossDeposition[surfaceHit]+weight;
            #endif
	            }
		}
            }
            //if(particles->test[indx] == 1.0)
            //{
            //    particles->test3[indx] = eInterpVal;
            //    particles->test[indx] = 2.0;
            //}
                //deposit on surface
            if(surface)
            {
            #if USE_CUDA > 0
                atomicAdd(&surfaces->sumWeightStrike[surfaceHit],weight);
                atomicAdd(&surfaces->sumParticlesStrike[surfaceHit],1);
            #else
                surfaces->sumWeightStrike[surfaceHit] =surfaces->sumWeightStrike[surfaceHit] +weight;
                surfaces->sumParticlesStrike[surfaceHit] = surfaces->sumParticlesStrike[surfaceHit]+1;
              //boundaryVector[wallHit].impacts = boundaryVector[wallHit].impacts +  particles->weight[indx];
            #endif
            #if FLUX_EA > 0
                EdistInd = std::floor((E0-E0dist)/dEdist);
                AdistInd = std::floor((thetaImpact-A0dist)/dAdist);
              
	        if((EdistInd >= 0) && (EdistInd < nEdist) && 
                  (AdistInd >= 0) && (AdistInd < nAdist))
                {
#if USE_CUDA > 0
                    atomicAdd(&surfaces->energyDistribution[surfaceHit*nEdist*nAdist + 
                                               EdistInd*nAdist + AdistInd], weight);
#else

                    surfaces->energyDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd] = 
                    surfaces->energyDistribution[surfaceHit*nEdist*nAdist + EdistInd*nAdist + AdistInd] +  weight;
#endif
        }
#endif
      }
      //reflect with weight and new initial conditions
      //std::cout << "particle wall hit Z and nwweight " << boundaryVector[wallHit].Z << " " << newWeight << std::endl;
      if (boundaryVector[wallHit].Z > 0.0 && newWeight > 0.0)
      //if(newWeight > 0.0)
      {
        particles->weight[indx] = newWeight;
        particles->hitWall[indx] = 0.0;
        particles->charge[indx] = 0.0;
        gitr_precision V0 = std::sqrt(2 * eInterpVal * 1.602e-19 / (particles->amu[indx] * 1.66e-27));
        particles->newVelocity[indx] = V0;
        vSampled[0] = V0 * std::sin(aInterpVal * 3.1415 / 180) * std::cos(2.0 * 3.1415 * r10);
        vSampled[1] = V0 * std::sin(aInterpVal * 3.1415 / 180) * std::sin(2.0 * 3.1415 * r10);
        vSampled[2] = V0 * std::cos(aInterpVal * 3.1415 / 180);
          //std::cout << "vSampled " << vSampled[0] << " " << vSampled[1] << " " << vSampled[2] << std::endl;
        boundaryVector[wallHit].transformToSurface(vSampled, particles->y[indx], particles->x[indx]);
        //float rr = std::sqrt(particles->x[indx]*particles->x[indx] + particles->y[indx]*particles->y[indx]);
        //if (particles->z[indx] < -4.1 && -signPartDotNormal*vSampled[0] > 0.0)
        //{
        //  std::cout << "particle index " << indx  << std::endl;
        //  std::cout << "aInterpVal" << aInterpVal  << std::endl;
        //  std::cout << "Surface Normal" << surfaceNormalVector[0] << " " << surfaceNormalVector[1] << " " << surfaceNormalVector[2] << std::endl;
        //  std::cout << "signPartDotNormal " << signPartDotNormal << std::endl;
        //  std::cout << "Particle hit wall with v " << vx << " " << vy << " " << vz<< std::endl;
          //std::cout << "vSampled " << vSampled[0] << " " << vSampled[1] << " " << vSampled[2] << std::endl;
        //  std::cout << "Final transform" << -signPartDotNormal*vSampled[0] << " " << -signPartDotNormal*vSampled[1] << " " << -signPartDotNormal*vSampled[2] << std::endl;
        //  std::cout << "Position of particle0 " << particles->xprevious[indx] << " " << particles->yprevious[indx] << " " << particles->zprevious[indx] << std::endl;
        //  std::cout << "Position of particle " << particles->x[indx] << " " << particles->y[indx] << " " << particles->z[indx] << std::endl;
          //std::cout << "normal vec " << surfaceNormalVector[0] << " " << surfaceNormalVector[1] << " " << surfaceNormalVector[2]<< " " << boundaryVector[wallHit].inDir << std::endl;
        //  }
        particles->vx[indx] = -static_cast<gitr_precision>(boundaryVector[wallHit].inDir)  * vSampled[0];
        particles->vy[indx] = -static_cast<gitr_precision>(boundaryVector[wallHit].inDir)  * vSampled[1];
        particles->vz[indx] = -static_cast<gitr_precision>(boundaryVector[wallHit].inDir)  * vSampled[2];
        //        //if(particles->test[indx] == 0.0)
        //        //{
        //        //    particles->test[indx] = 1.0;
        //        //    particles->test0[indx] = aInterpVal;
        //        //    particles->test1[indx] = eInterpVal;
        //        //    particles->test2[indx] = V0;
        //        //    particles->test3[indx] = vSampled[2];
        //        //}

        particles->xprevious[indx] = particles->x[indx] - static_cast<gitr_precision>(boundaryVector[wallHit].inDir) * surfaceNormalVector[0] * 1e-4;
        particles->yprevious[indx] = particles->y[indx] - static_cast<gitr_precision>(boundaryVector[wallHit].inDir) * surfaceNormalVector[1] * 1e-4;
      particles->zprevious[indx] = particles->z[indx] - static_cast<gitr_precision>(boundaryVector[wallHit].inDir) * surfaceNormalVector[2] * 1e-4;
        //std::cout << "New vel " << particles->vx[indx] << " " << particles->vy[indx] << " " << particles->vz[indx] << std::endl;
        //std::cout << "New pos " << particles->xprevious[indx] << " " << particles->yprevious[indx] << " " << particles->zprevious[indx] << std::endl;
        //if(particles->test[indx] == 0.0)
        //{
        //    particles->test[indx] = 1.0;
        //    particles->test0[indx] = particles->x[indx];
        //    particles->test1[indx] = particles->y[indx];
        //    particles->test2[indx] = particles->z[indx];
        //    particles->test3[indx] = signPartDotNormal;
        //}
        //else
        //{
        //    particles->test[indx] = particles->test[indx] + 1.0;
        //}
      } else {
        particles->hitWall[indx] = 2.0;
      }
    }
  }
