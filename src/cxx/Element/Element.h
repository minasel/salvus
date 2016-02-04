//
// Created by Michael Afanasiev on 2016-01-29.
//

#ifndef SALVUS_ELEMENT_H
#define SALVUS_ELEMENT_H


#include <petscsys.h>
#include <petscvec.h>
#include <petscdmtypes.h>
#include <vector>
#include <Eigen/Dense>
#include "../Options.h"
#include "../Model/ExodusModel.h"
#include "../Source.h"

class Element {

private:

    PetscInt mLocalElementNumber;

protected:

    PetscInt mPolynomialOrder;
    PetscInt mNumberDimensions;
    PetscInt mNumberDofVertex;
    PetscInt mNumberDofEdge;
    PetscInt mNumberDofFace;
    PetscInt mNumberDofVolume;
    PetscInt mNumberIntegrationPoints;
    PetscInt mNumberVertex;

    std::string mElementShape;
    std::string mPhysicsSystem;
    std::vector<Source*> mSources;

    DM mDistributedMesh;
    PetscSection mMeshSection;

public:

    // Utility methods.
    static Element *factory(Options options);
    virtual Element* clone() const = 0;
    void printInfoToScreen() const;

    // Generic methods.
    void registerMesh(DM &distributed_mesh, PetscSection &section);

    void __gatherDistributedFieldsToPartition(Vec &local_field_vec,
                                              Vec &global_field_vec);
    void __gatherPartitionFieldsToElement(Vec &local_field_vec,
                                          Eigen::VectorXd &element_field_vec,
                                          std::vector<int> &closure_mapping);
    void __scatterElementFieldsToPartition(Vec &local_field_vec,
                                           const Eigen::VectorXd &element_field_vec);
    void __scatterPartitionFieldsToDistributedEnd(Vec &local_field_vec, Vec &global_field_vec);
    void __scatterPartitionFieldsToDistributedBegin(Vec &local_field_vec, Vec &global_field_vec);

    // Pure virtual methods.
    virtual void attachSource(std::vector<Source*> sources) = 0;
    virtual void readOperators() = 0;
    virtual void registerFieldVectors() = 0;
    virtual void attachVertexCoordinates() = 0;
    virtual void attachIntegrationPoints() = 0;
    virtual void constructStiffnessMatrix() = 0;
    virtual void gatherPartitionFieldsToElement() = 0;
    virtual void scatterElementFieldsToPartition() = 0;
    virtual void gatherDistributedFieldsToPartition() = 0;
    virtual void scatterPartitionFieldsToDistributedEnd() = 0;
    virtual void scatterPartitionFieldsToDistributedBegin() = 0;
    virtual void interpolateMaterialProperties(ExodusModel &model) = 0;

    // Integer setattr.
    inline void SetLocalElementNumber(PetscInt number) { mLocalElementNumber = number; }

    // Integer geters.
    inline PetscInt NumberDimensions() const { return mNumberDimensions; }
    inline PetscInt NumberDofVertex() const { return mNumberDofVertex; }
    inline PetscInt NumberDofEdge() const { return mNumberDofEdge; }
    inline PetscInt NumberDofFace() const { return mNumberDofFace; }
    inline PetscInt NumberDofVolume() const { return mNumberDofVolume; }
    inline PetscInt LocalElementNumber() const { return mLocalElementNumber; }
    inline PetscInt NumberIntegrationPoints() const { return mNumberIntegrationPoints; }


};


#endif //SALVUS_ELEMENT_H
