"""Structural regression tests for real playable scenario positioning."""
from __future__ import annotations
from pathlib import Path
import unittest

TOOLS=Path(__file__).resolve().parents[1]
ROOT=TOOLS.parents[1]
SRC=(ROOT/'Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp').read_text()


class RealSourceContract(unittest.TestCase):
    def test_broken_five_fan_removed(self):
        self.assertNotIn('for(double Deg:{0.,25.,-25.,50.,-50.})',SRC)
        self.assertNotIn('Forward.RotateAngleAxis(Deg,FVector::UpVector)*1100.',SRC)
    def test_actual_perception_geometry_not_head_guess(self):
        self.assertIn('PedroDirectObservation::CheckGeometry(Eye,Facing,Candidate,1500.f,75.f)',SRC)
        self.assertIn('S.Player->GetActorLocation(),1500.f,75.f)',SRC)
        self.assertIn('PlayerMovement->GetFeetLocation()',SRC)
    def test_full_recast_path_and_real_line_trace(self):
        for proof in ('ProjectPointToNavigation(Wish,NavPoint','FindPathSync(AgentProperties,Query)',
                      'Query.SetAllowPartialPaths(false)', 'Path.Path->IsPartial()',
                      'Path.Path->ContainsAnyCustomLink()', 'Path.Path->GetLength()>2200.f',
                      'LineTraceSingleByChannel(Block,Eye,Candidate,ECC_Visibility,QueryParams)',
                      'OverlapBlockingTestByChannel(Candidate,Player->GetActorQuat()'):
            self.assertIn(proof,SRC)
    def test_authority_guards_and_no_fake_pedro_movement(self):
        part=SRC.split('bool FindPlayerSetupLocation(',1)[1].split('bool Finish(',1)[0]
        self.assertNotIn('Observer->SetActorLocation',part)
        self.assertNotIn('Observer->SetActorRotation',part)
        self.assertNotIn('AcceptSighting',part)
        self.assertNotIn('OnPlayerMeleeHit',part)
        self.assertNotIn('status=PASS',part)
        self.assertIn('SetActorLocation(Candidate,false,nullptr,ETeleportType::TeleportPhysics)',SRC)
        self.assertIn('if(Travel>120.0)',SRC)
        self.assertIn('S.Controller->GetPedroMoveSubmissions()',SRC)
    def test_bounded_angles_radii_and_precise_diagnostics(self):
        self.assertIn('Requests.Num()>96',SRC)
        self.assertIn('Radii[]={1100.f,950.f,1250.f,800.f,700.f,1350.f}',SRC)
        self.assertIn('Angles[]={0.f,15.f,-15.f,30.f,-30.f,45.f,-45.f,',SRC)
        self.assertIn('NoVisibleReachablePlayerTestPosition',SRC)
        for key in ('projected=%d','complete_paths=%d','occluded=%d','occupied=%d','geometry_rejected=%d'):
            self.assertIn(key,SRC)
        self.assertNotIn('PEDRO_PLAYABLE_TEST status=PASS case=Setup',SRC)


if __name__=='__main__':unittest.main()
